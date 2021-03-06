//===-- llvm/Attributes.h - Container for Attributes ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file contains the simple types necessary to represent the
/// attributes associated with functions and their calls.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_ATTRIBUTES_H
#define LLVM_ATTRIBUTES_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/MathExtras.h"
#include <cassert>
#include <string>

namespace llvm {

class AttrBuilder;
class AttributeImpl;
class LLVMContext;
class Type;

//===----------------------------------------------------------------------===//
/// \class
/// \brief Functions, function parameters, and return types can have attributes
/// to indicate how they should be treated by optimizations and code
/// generation. This class represents one of those attributes. It's light-weight
/// and should be passed around by-value.
class Attribute {
public:
  /// This enumeration lists the attributes that can be associated with
  /// parameters, function results or the function itself.
  ///
  /// Note: uwtable is about the ABI or the user mandating an entry in the
  /// unwind table. The nounwind attribute is about an exception passing by the
  /// function.
  ///
  /// In a theoretical system that uses tables for profiling and sjlj for
  /// exceptions, they would be fully independent. In a normal system that uses
  /// tables for both, the semantics are:
  ///
  /// nil                = Needs an entry because an exception might pass by.
  /// nounwind           = No need for an entry
  /// uwtable            = Needs an entry because the ABI says so and because
  ///                      an exception might pass by.
  /// uwtable + nounwind = Needs an entry because the ABI says so.

  enum AttrKind {
    // IR-Level Attributes
    None,                  ///< No attributes have been set
    AddressSafety,         ///< Address safety checking is on.
    Alignment,             ///< Alignment of parameter (5 bits)
                           ///< stored as log2 of alignment with +1 bias
                           ///< 0 means unaligned (different from align(1))
    AlwaysInline,          ///< inline=always
    ByVal,                 ///< Pass structure by value
    InlineHint,            ///< Source said inlining was desirable
    InReg,                 ///< Force argument to be passed in register
    MinSize,               ///< Function must be optimized for size first
    Naked,                 ///< Naked function
    Nest,                  ///< Nested function static chain
    NoAlias,               ///< Considered to not alias after call
    NoCapture,             ///< Function creates no aliases of pointer
    NoDuplicate,           ///< Call cannot be duplicated
    NoImplicitFloat,       ///< Disable implicit floating point insts
    NoInline,              ///< inline=never
    NonLazyBind,           ///< Function is called early and/or
                           ///< often, so lazy binding isn't worthwhile
    NoRedZone,             ///< Disable redzone
    NoReturn,              ///< Mark the function as not returning
    NoUnwind,              ///< Function doesn't unwind stack
    OptimizeForSize,       ///< opt_size
    ReadNone,              ///< Function does not access memory
    ReadOnly,              ///< Function only reads from memory
    ReturnsTwice,          ///< Function can return twice
    SExt,                  ///< Sign extended before/after call
    StackAlignment,        ///< Alignment of stack for function (3 bits)
                           ///< stored as log2 of alignment with +1 bias 0
                           ///< means unaligned (different from
                           ///< alignstack=(1))
    StackProtect,          ///< Stack protection.
    StackProtectReq,       ///< Stack protection required.
    StructRet,             ///< Hidden pointer to structure to return
    UWTable,               ///< Function must be in a unwind table
    ZExt                   ///< Zero extended before/after call
  };
private:
  AttributeImpl *pImpl;
  Attribute(AttributeImpl *A) : pImpl(A) {}
public:
  Attribute() : pImpl(0) {}

  /// \brief Return a uniquified Attribute object. This takes the uniquified
  /// value from the Builder and wraps it in the Attribute class.
  static Attribute get(LLVMContext &Context, ArrayRef<AttrKind> Vals);
  static Attribute get(LLVMContext &Context, AttrBuilder &B);

  /// \brief Return true if the attribute is present.
  bool hasAttribute(AttrKind Val) const;

  /// \brief Return true if attributes exist
  bool hasAttributes() const;

  /// \brief Return true if the attributes are a non-null intersection.
  bool hasAttributes(const Attribute &A) const;

  /// \brief Returns the alignment field of an attribute as a byte alignment
  /// value.
  unsigned getAlignment() const;

  /// \brief Returns the stack alignment field of an attribute as a byte
  /// alignment value.
  unsigned getStackAlignment() const;

  bool operator==(AttrKind K) const;
  bool operator!=(AttrKind K) const;

  // FIXME: Remove these 'operator' methods.
  bool operator==(const Attribute &A) const {
    return pImpl == A.pImpl;
  }
  bool operator!=(const Attribute &A) const {
    return pImpl != A.pImpl;
  }

  uint64_t getBitMask() const;

  /// \brief Which attributes cannot be applied to a type.
  static Attribute typeIncompatible(Type *Ty);

  /// \brief This returns an integer containing an encoding of all the LLVM
  /// attributes found in the given attribute bitset.  Any change to this
  /// encoding is a breaking change to bitcode compatibility.
  static uint64_t encodeLLVMAttributesForBitcode(Attribute Attrs);

  /// \brief This returns an attribute bitset containing the LLVM attributes
  /// that have been decoded from the given integer.  This function must stay in
  /// sync with 'encodeLLVMAttributesForBitcode'.
  static Attribute decodeLLVMAttributesForBitcode(LLVMContext &C,
                                                  uint64_t EncodedAttrs);

  /// \brief The Attribute is converted to a string of equivalent mnemonic. This
  /// is, presumably, for writing out the mnemonics for the assembly writer.
  std::string getAsString() const;
};

//===----------------------------------------------------------------------===//
/// \class
/// \brief This class is used in conjunction with the Attribute::get method to
/// create an Attribute object. The object itself is uniquified. The Builder's
/// value, however, is not. So this can be used as a quick way to test for
/// equality, presence of attributes, etc.
class AttrBuilder {
  uint64_t Bits;
public:
  AttrBuilder() : Bits(0) {}
  explicit AttrBuilder(uint64_t B) : Bits(B) {}
  AttrBuilder(const Attribute &A) : Bits(A.getBitMask()) {}

  void clear() { Bits = 0; }

  /// addAttribute - Add an attribute to the builder.
  AttrBuilder &addAttribute(Attribute::AttrKind Val);

  /// removeAttribute - Remove an attribute from the builder.
  AttrBuilder &removeAttribute(Attribute::AttrKind Val);

  /// addAttribute - Add the attributes from A to the builder.
  AttrBuilder &addAttributes(const Attribute &A);

  /// removeAttribute - Remove the attributes from A from the builder.
  AttrBuilder &removeAttributes(const Attribute &A);

  /// \brief Return true if the builder has the specified attribute.
  bool contains(Attribute::AttrKind A) const;

  /// hasAttributes - Return true if the builder has IR-level attributes.
  bool hasAttributes() const;

  /// hasAttributes - Return true if the builder has any attribute that's in the
  /// specified attribute.
  bool hasAttributes(const Attribute &A) const;

  /// hasAlignmentAttr - Return true if the builder has an alignment attribute.
  bool hasAlignmentAttr() const;

  /// getAlignment - Retrieve the alignment attribute, if it exists.
  uint64_t getAlignment() const;

  /// getStackAlignment - Retrieve the stack alignment attribute, if it exists.
  uint64_t getStackAlignment() const;

  /// addAlignmentAttr - This turns an int alignment (which must be a power of
  /// 2) into the form used internally in Attribute.
  AttrBuilder &addAlignmentAttr(unsigned Align);

  /// addStackAlignmentAttr - This turns an int stack alignment (which must be a
  /// power of 2) into the form used internally in Attribute.
  AttrBuilder &addStackAlignmentAttr(unsigned Align);

  /// addRawValue - Add the raw value to the internal representation.
  /// N.B. This should be used ONLY for decoding LLVM bitcode!
  AttrBuilder &addRawValue(uint64_t Val);

  /// @brief Remove attributes that are used on functions only.
  void removeFunctionOnlyAttrs() {
    removeAttribute(Attribute::NoReturn)
      .removeAttribute(Attribute::NoUnwind)
      .removeAttribute(Attribute::ReadNone)
      .removeAttribute(Attribute::ReadOnly)
      .removeAttribute(Attribute::NoInline)
      .removeAttribute(Attribute::AlwaysInline)
      .removeAttribute(Attribute::OptimizeForSize)
      .removeAttribute(Attribute::StackProtect)
      .removeAttribute(Attribute::StackProtectReq)
      .removeAttribute(Attribute::NoRedZone)
      .removeAttribute(Attribute::NoImplicitFloat)
      .removeAttribute(Attribute::Naked)
      .removeAttribute(Attribute::InlineHint)
      .removeAttribute(Attribute::StackAlignment)
      .removeAttribute(Attribute::UWTable)
      .removeAttribute(Attribute::NonLazyBind)
      .removeAttribute(Attribute::ReturnsTwice)
      .removeAttribute(Attribute::AddressSafety)
      .removeAttribute(Attribute::MinSize)
      .removeAttribute(Attribute::NoDuplicate);
  }

  uint64_t getBitMask() const { return Bits; }

  bool operator==(const AttrBuilder &B) {
    return Bits == B.Bits;
  }
  bool operator!=(const AttrBuilder &B) {
    return Bits != B.Bits;
  }
};

//===----------------------------------------------------------------------===//
/// \class
/// \brief This is just a pair of values to associate a set of attributes with
/// an index.
struct AttributeWithIndex {
  Attribute Attrs;  ///< The attributes that are set, or'd together.
  unsigned Index;   ///< Index of the parameter for which the attributes apply.
                    ///< Index 0 is used for return value attributes.
                    ///< Index ~0U is used for function attributes.

  static AttributeWithIndex get(LLVMContext &C, unsigned Idx,
                                ArrayRef<Attribute::AttrKind> Attrs) {
    return get(Idx, Attribute::get(C, Attrs));
  }
  static AttributeWithIndex get(unsigned Idx, Attribute Attrs) {
    AttributeWithIndex P;
    P.Index = Idx;
    P.Attrs = Attrs;
    return P;
  }
};

//===----------------------------------------------------------------------===//
// AttributeSet Smart Pointer
//===----------------------------------------------------------------------===//

class AttributeSetImpl;

//===----------------------------------------------------------------------===//
/// \class
/// \brief This class manages the ref count for the opaque AttributeSetImpl
/// object and provides accessors for it.
class AttributeSet {
public:
  enum AttrIndex {
    ReturnIndex = 0U,
    FunctionIndex = ~0U
  };
private:
  /// \brief The attributes that we are managing.  This can be null to represent
  /// the empty attributes list.
  AttributeSetImpl *AttrList;

  /// \brief The attributes for the specified index are returned.  Attributes
  /// for the result are denoted with Idx = 0.
  Attribute getAttributes(unsigned Idx) const;

  explicit AttributeSet(AttributeSetImpl *LI) : AttrList(LI) {}
public:
  AttributeSet() : AttrList(0) {}
  AttributeSet(const AttributeSet &P) : AttrList(P.AttrList) {}
  const AttributeSet &operator=(const AttributeSet &RHS);

  //===--------------------------------------------------------------------===//
  // Attribute List Construction and Mutation
  //===--------------------------------------------------------------------===//

  /// \brief Return an AttributeSet with the specified parameters in it.
  static AttributeSet get(LLVMContext &C, ArrayRef<AttributeWithIndex> Attrs);

  /// \brief Add the specified attribute at the specified index to this
  /// attribute list.  Since attribute lists are immutable, this returns the new
  /// list.
  AttributeSet addAttr(LLVMContext &C, unsigned Idx, Attribute Attrs) const;

  /// \brief Remove the specified attribute at the specified index from this
  /// attribute list.  Since attribute lists are immutable, this returns the new
  /// list.
  AttributeSet removeAttr(LLVMContext &C, unsigned Idx, Attribute Attrs) const;

  //===--------------------------------------------------------------------===//
  // Attribute Set Accessors
  //===--------------------------------------------------------------------===//

  /// \brief The attributes for the specified index are returned.
  Attribute getParamAttributes(unsigned Idx) const {
    return getAttributes(Idx);
  }

  /// \brief The attributes for the ret value are returned.
  Attribute getRetAttributes() const {
    return getAttributes(ReturnIndex);
  }

  /// \brief The function attributes are returned.
  Attribute getFnAttributes() const {
    return getAttributes(FunctionIndex);
  }

  /// \brief Return the alignment for the specified function parameter.
  unsigned getParamAlignment(unsigned Idx) const {
    return getAttributes(Idx).getAlignment();
  }

  /// \brief Return true if the attribute exists at the given index.
  bool hasAttribute(unsigned Index, Attribute::AttrKind Kind) const;

  /// \brief Return true if attribute exists at the given index.
  bool hasAttributes(unsigned Index) const;

  /// \brief Get the stack alignment.
  unsigned getStackAlignment(unsigned Index) const;

  /// \brief Return the attributes at the index as a string.
  std::string getAsString(unsigned Index) const;

  uint64_t getBitMask(unsigned Index) const;

  /// \brief Return true if the specified attribute is set for at least one
  /// parameter or for the return value.
  bool hasAttrSomewhere(Attribute::AttrKind Attr) const;

  /// operator==/!= - Provide equality predicates.
  bool operator==(const AttributeSet &RHS) const {
    return AttrList == RHS.AttrList;
  }
  bool operator!=(const AttributeSet &RHS) const {
    return AttrList != RHS.AttrList;
  }

  //===--------------------------------------------------------------------===//
  // Attribute Set Introspection
  //===--------------------------------------------------------------------===//

  /// \brief Return a raw pointer that uniquely identifies this attribute list.
  void *getRawPointer() const {
    return AttrList;
  }

  // Attributes are stored as a dense set of slots, where there is one slot for
  // each argument that has an attribute.  This allows walking over the dense
  // set instead of walking the sparse list of attributes.

  /// \brief Return true if there are no attributes.
  bool isEmpty() const {
    return AttrList == 0;
  }

  /// \brief Return the number of slots used in this attribute list.  This is
  /// the number of arguments that have an attribute set on them (including the
  /// function itself).
  unsigned getNumSlots() const;

  /// \brief Return the AttributeWithIndex at the specified slot.  This holds a
  /// index number plus a set of attributes.
  const AttributeWithIndex &getSlot(unsigned Slot) const;

  void dump() const;
};

} // end llvm namespace

#endif
