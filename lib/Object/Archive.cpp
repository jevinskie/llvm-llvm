//===- Archive.cpp - ar File Format implementation --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the ArchiveObjectFile class.
//
//===----------------------------------------------------------------------===//

#include "llvm/Object/Archive.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace llvm;
using namespace object;

static const char *Magic = "!<arch>\n";

namespace {
struct ArchiveMemberHeader {
  char Name[16];
  char LastModified[12];
  char UID[6];
  char GID[6];
  char AccessMode[8];
  char Size[10]; ///< Size of data, not including header or padding.
  char Terminator[2];

  ///! Get the name without looking up long names.
  StringRef getName() const {
    char EndCond;
    if (Name[0] == '/' || Name[0] == '#')
      EndCond = ' ';
    else
      EndCond = '/';
    StringRef::size_type end = StringRef(Name, sizeof(Name)).find(EndCond);
    if (end == StringRef::npos)
      end = sizeof(Name);
    assert(end <= sizeof(Name) && end > 0);
    // Don't include the EndCond if there is one.
    return StringRef(Name, end);
  }

  uint64_t getSize() const {
    uint64_t ret;
    StringRef(Size, sizeof(Size)).getAsInteger(10, ret);
    return ret;
  }
};
}

static const ArchiveMemberHeader *ToHeader(const char *base) {
  return reinterpret_cast<const ArchiveMemberHeader *>(base);
}


static bool isInternalMember(const ArchiveMemberHeader &amh) {
  static const char *const internals[] = {
    "/",
    "//",
    "#_LLVM_SYM_TAB_#"
  };

  StringRef name = amh.getName();
  for (std::size_t i = 0; i < sizeof(internals) / sizeof(*internals); ++i) {
    if (name == internals[i])
      return true;
  }
  return false;
}

void Archive::anchor() { }

Archive::Child Archive::Child::getNext() const {
  size_t SpaceToSkip = sizeof(ArchiveMemberHeader) +
    ToHeader(Data.data())->getSize();
  // If it's odd, add 1 to make it even.
  if (SpaceToSkip & 1)
    ++SpaceToSkip;

  const char *NextLoc = Data.data() + SpaceToSkip;

  // Check to see if this is past the end of the archive.
  if (NextLoc >= Parent->Data->getBufferEnd())
    return Child(Parent, StringRef(0, 0));

  size_t NextSize = sizeof(ArchiveMemberHeader) +
    ToHeader(NextLoc)->getSize();

  return Child(Parent, StringRef(NextLoc, NextSize));
}

error_code Archive::Child::getName(StringRef &Result) const {
  StringRef name = ToHeader(Data.data())->getName();
  // Check if it's a special name.
  if (name[0] == '/') {
    if (name.size() == 1) { // Linker member.
      Result = name;
      return object_error::success;
    }
    if (name.size() == 2 && name[1] == '/') { // String table.
      Result = name;
      return object_error::success;
    }
    // It's a long name.
    // Get the offset.
    uint64_t offset;
    name.substr(1).getAsInteger(10, offset);
    const char *addr = Parent->StringTable->Data.begin()
                       + sizeof(ArchiveMemberHeader)
                       + offset;
    // Verify it.
    if (Parent->StringTable == Parent->end_children()
        || addr < (Parent->StringTable->Data.begin()
                   + sizeof(ArchiveMemberHeader))
        || addr > (Parent->StringTable->Data.begin()
                   + sizeof(ArchiveMemberHeader)
                   + Parent->StringTable->getSize()))
      return object_error::parse_failed;

    // GNU long file names end with a /.
    if (Parent->kind() == K_GNU) {
      StringRef::size_type End = StringRef(addr).find('/');
      Result = StringRef(addr, End);
    } else {
      Result = addr;
    }
    return object_error::success;
  } else if (name.startswith("#1/")) {
    uint64_t name_size;
    name.substr(3).getAsInteger(10, name_size);
    Result = Data.substr(0, name_size);
    return object_error::success;
  }
  // It's a simple name.
  if (name[name.size() - 1] == '/')
    Result = name.substr(0, name.size() - 1);
  else
    Result = name;
  return object_error::success;
}

uint64_t Archive::Child::getSize() const {
  uint64_t size = ToHeader(Data.data())->getSize();
  // Don't include attached name.
  StringRef name =  ToHeader(Data.data())->getName();
  if (name.startswith("#1/")) {
    uint64_t name_size;
    name.substr(3).getAsInteger(10, name_size);
    size -= name_size;
  }
  return size;
}

MemoryBuffer *Archive::Child::getBuffer() const {
  StringRef name;
  if (getName(name)) return NULL;
  int size = sizeof(ArchiveMemberHeader);
  if (name.startswith("#1/")) {
    uint64_t name_size;
    name.substr(3).getAsInteger(10, name_size);
    size += name_size;
  }
  return MemoryBuffer::getMemBuffer(Data.substr(size, getSize()),
                                    name,
                                    false);
}

error_code Archive::Child::getAsBinary(OwningPtr<Binary> &Result) const {
  OwningPtr<Binary> ret;
  if (error_code ec =
    createBinary(getBuffer(), ret))
    return ec;
  Result.swap(ret);
  return object_error::success;
}

Archive::Archive(MemoryBuffer *source, error_code &ec)
  : Binary(Binary::ID_Archive, source) {
  // Check for sufficient magic.
  if (!source || source->getBufferSize()
                 < (8 + sizeof(ArchiveMemberHeader) + 2) // Smallest archive.
              || StringRef(source->getBufferStart(), 8) != Magic) {
    ec = object_error::invalid_file_type;
    return;
  }

  // Get the special members.
  child_iterator i = begin_children(false);
  child_iterator e = end_children();

  StringRef name;
  if ((ec = i->getName(name)))
    return;

  // Below is the pattern that is used to figure out the archive format
  // GNU archive format
  //  First member : / (points to the symbol table )
  //  Second member : // (may exist, if it exists, points to the string table)
  //  Note : The string table is used if the filename exceeds 15 characters
  // BSD archive format
  //  First member : __.SYMDEF (points to the symbol table)
  //  There is no string table, if the filename exceeds 15 characters or has a 
  //  embedded space, the filename has #1/<size>, The size represents the size 
  //  of the filename that needs to be read after the archive header
  // COFF archive format
  //  First member : /
  //  Second member : / (provides a directory of symbols)
  //  Third member : // contains the string table, this is present even if the
  //                    string table is empty
  if (name == "/") {
    SymbolTable = i;
    StringTable = e;
    if (i != e) ++i;
    if ((ec = i->getName(name)))
      return;
    if (name[0] != '/') {
      Format = K_GNU;
    } else if ((name.size() > 1) && (name == "//")) { 
      Format = K_GNU;
      StringTable = i;
      ++i;
    } else  { 
      Format = K_COFF;
      if (i != e) {
        SymbolTable = i;
        ++i;
      }
      if (i != e) {
        StringTable = i;
      }
    }
  } else if (name == "__.SYMDEF") {
    Format = K_BSD;
    SymbolTable = i;
    StringTable = e;
  } 
  ec = object_error::success;
}

Archive::child_iterator Archive::begin_children(bool skip_internal) const {
  const char *Loc = Data->getBufferStart() + strlen(Magic);
  size_t Size = sizeof(ArchiveMemberHeader) +
    ToHeader(Loc)->getSize();
  Child c(this, StringRef(Loc, Size));
  // Skip internals at the beginning of an archive.
  if (skip_internal && isInternalMember(*ToHeader(Loc)))
    return c.getNext();
  return c;
}

Archive::child_iterator Archive::end_children() const {
  return Child(this, StringRef(0, 0));
}

error_code Archive::Symbol::getName(StringRef &Result) const {
  Result =
    StringRef(Parent->SymbolTable->getBuffer()->getBufferStart() + StringIndex);
  return object_error::success;
}

error_code Archive::Symbol::getMember(child_iterator &Result) const {
  const char *Buf = Parent->SymbolTable->getBuffer()->getBufferStart();
  const char *Offsets = Buf + 4;
  uint32_t Offset = 0;
  if (Parent->kind() == K_GNU) {
    Offset = *(reinterpret_cast<const support::ubig32_t*>(Offsets)
               + SymbolIndex);
  } else if (Parent->kind() == K_BSD) {
    llvm_unreachable("BSD format is not supported");
  } else {
    uint32_t MemberCount = *reinterpret_cast<const support::ulittle32_t*>(Buf);
    
    // Skip offsets.
    Buf += sizeof(support::ulittle32_t)
           + (MemberCount * sizeof(support::ulittle32_t));

    uint32_t SymbolCount = *reinterpret_cast<const support::ulittle32_t*>(Buf);

    if (SymbolIndex >= SymbolCount)
      return object_error::parse_failed;

    // Skip SymbolCount to get to the indices table.
    const char *Indices = Buf + sizeof(support::ulittle32_t);

    // Get the index of the offset in the file member offset table for this
    // symbol.
    uint16_t OffsetIndex =
      *(reinterpret_cast<const support::ulittle16_t*>(Indices)
        + SymbolIndex);
    // Subtract 1 since OffsetIndex is 1 based.
    --OffsetIndex;

    if (OffsetIndex >= MemberCount)
      return object_error::parse_failed;

    Offset = *(reinterpret_cast<const support::ulittle32_t*>(Offsets)
               + OffsetIndex);
  }

  const char *Loc = Parent->getData().begin() + Offset;
  size_t Size = sizeof(ArchiveMemberHeader) +
    ToHeader(Loc)->getSize();
  Result = Child(Parent, StringRef(Loc, Size));

  return object_error::success;
}

Archive::Symbol Archive::Symbol::getNext() const {
  Symbol t(*this);
  // Go to one past next null.
  t.StringIndex =
    Parent->SymbolTable->getBuffer()->getBuffer().find('\0', t.StringIndex) + 1;
  ++t.SymbolIndex;
  return t;
}

Archive::symbol_iterator Archive::begin_symbols() const {
  const char *buf = SymbolTable->getBuffer()->getBufferStart();
  if (kind() == K_GNU) {
    uint32_t symbol_count = 0;
    symbol_count = *reinterpret_cast<const support::ubig32_t*>(buf);
    buf += sizeof(uint32_t) + (symbol_count * (sizeof(uint32_t)));
  } else if (kind() == K_BSD) {
    llvm_unreachable("BSD archive format is not supported");
  } else {
    uint32_t member_count = 0;
    uint32_t symbol_count = 0;
    member_count = *reinterpret_cast<const support::ulittle32_t*>(buf);
    buf += 4 + (member_count * 4); // Skip offsets.
    symbol_count = *reinterpret_cast<const support::ulittle32_t*>(buf);
    buf += 4 + (symbol_count * 2); // Skip indices.
  }
  uint32_t string_start_offset =
    buf - SymbolTable->getBuffer()->getBufferStart();
  return symbol_iterator(Symbol(this, 0, string_start_offset));
}

Archive::symbol_iterator Archive::end_symbols() const {
  const char *buf = SymbolTable->getBuffer()->getBufferStart();
  uint32_t symbol_count = 0;
  if (kind() == K_GNU) {
    symbol_count = *reinterpret_cast<const support::ubig32_t*>(buf);
    buf += sizeof(uint32_t) + (symbol_count * (sizeof(uint32_t)));
  } else if (kind() == K_BSD) {
    llvm_unreachable("BSD archive format is not supported");
  } else {
    uint32_t member_count = 0;
    member_count = *reinterpret_cast<const support::ulittle32_t*>(buf);
    buf += 4 + (member_count * 4); // Skip offsets.
    symbol_count = *reinterpret_cast<const support::ulittle32_t*>(buf);
  }
  return symbol_iterator(
    Symbol(this, symbol_count, 0));
}

Archive::child_iterator Archive::findSym(StringRef name) const {
  Archive::symbol_iterator bs = begin_symbols();
  Archive::symbol_iterator es = end_symbols();
  Archive::child_iterator result;
  
  StringRef symname;
  for (; bs != es; ++bs) {
    if (bs->getName(symname))
        return end_children();
    if (symname == name) {
      if (bs->getMember(result))
        return end_children();
      return result;
    }
  }
  return end_children();
}
