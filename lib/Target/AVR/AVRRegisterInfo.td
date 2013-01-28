class AVRGPReg<string n, bits<5> num> : Register<n> {
	field bits<5> Num;
	let Num = num;
	let Namespace = "AVR";
}

class AVRIndAddrReg<string n, bits<5> num, list<Register> subregs> : AVRGPReg<n, num> {
	let Num = num;
	let SubRegs = subregs;
	let Namespace = "AVR";
}

let Namespace = "AVR" in {
	def R0	: AVRGPReg<"r0", 0>;
	def R1	: AVRGPReg<"r1", 1>;
	def R2	: AVRGPReg<"r2", 2>;
	def R3	: AVRGPReg<"r3", 3>;
	def R4	: AVRGPReg<"r4", 4>;
	def R5	: AVRGPReg<"r5", 5>;
	def R6	: AVRGPReg<"r6", 6>;
	def R7	: AVRGPReg<"r7", 7>;
	def R8	: AVRGPReg<"r8", 8>;
	def R9	: AVRGPReg<"r9", 9>;
	def R10	: AVRGPReg<"r10", 10>;
	def R11	: AVRGPReg<"r11", 11>;
	def R12	: AVRGPReg<"r12", 12>;
	def R13	: AVRGPReg<"r13", 13>;
	def R14	: AVRGPReg<"r14", 14>;
	def R15	: AVRGPReg<"r15", 15>;
	def R16	: AVRGPReg<"r16", 16>;
	def R17	: AVRGPReg<"r17", 17>;
	def R18	: AVRGPReg<"r18", 18>;
	def R19	: AVRGPReg<"r19", 19>;
	def R20	: AVRGPReg<"r20", 20>;
	def R21	: AVRGPReg<"r21", 21>;
	def R22	: AVRGPReg<"r22", 22>;
	def R23	: AVRGPReg<"r23", 23>;
	def R24	: AVRGPReg<"r24", 24>;
	def R25	: AVRGPReg<"r25", 25>;
	def R26	: AVRGPReg<"r26", 26>;
	def R27	: AVRGPReg<"r27", 27>;
	def R28	: AVRGPReg<"r28", 28>;
	def R29	: AVRGPReg<"r29", 29>;
	def R30	: AVRGPReg<"r30", 30>;
	def R31	: AVRGPReg<"r31", 31>;
	def X	: AVRIndAddrReg<"X", 26, [R26,R27]>;
	def Y	: AVRIndAddrReg<"X", 28, [R28,R29]>;
	def Z	: AVRIndAddrReg<"X", 30, [R30,R31]>;
}

def GPRegs RegisterClass<"AVR", [i8], 8,
	(add R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R16,
		 R17, R18, R19, R20, R21, R22, R23, R24, R25, R26, R27, R28, R29, R30,
		 R31)>;

def IndAddrRegs RegisterClass<"AVR", [i16], 16,
	(add X, Y, Z)>;



	

