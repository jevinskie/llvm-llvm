include "llvm/Target/Target.td"

include "AVRRegisterInfo.td"
include "AVRSchedule.td"
include "AVRInstrInfo.td"

def AVRInstrInfo : InstrInfo;

def AVR : Target {
	let InstructionSet = AVRInstrInfo;
}

