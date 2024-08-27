#ifndef _DECODER_H
#define _DECODER_H

typedef enum Reg {
  NO_REG,
  AL,
  AX,
  CL,
  CX,
  DL,
  DX,
  BL,
  BX,
  AH,
  SP,
  CH,
  BP,
  DH,
  SI,
  BH,
  DI,
} Reg;

typedef struct DirectAddr {
  int addr;
} DirectAddr;

typedef struct EffectiveAddr {
  /** first register of the equation */
  Reg operand1;
  /** second register of the equation, nullable */
  Reg operand2;
  /** resolved value (from displacement), nullable */
  int operand3;
} EffectiveAddr;

typedef struct Register {
  Reg r;
} Register;

typedef struct Immediate {
  int val;
} Immediate;

typedef enum OperandType {
  DIRECT_ADDR,
  EFFECTIVE_ADDR,
  REGISTER,
  IMMEDIATE
} OperandType;

typedef union OperandData {
  DirectAddr addr;
  EffectiveAddr e_addr;
  Register reg;
  Immediate imm;
} OperandData;

typedef struct Operand {
  OperandType t;
  OperandData operand;
} Operand;

typedef enum Mod {
  MEM_NO_DISP,
  MEM_DISP_8,
  MEM_DISP_16,
  REG,
} Mod;

typedef struct MovOp {
  Operand src;
  Operand dst;
} MovOp;

typedef struct UnknownOp {
} UnknownOp;

typedef struct AddOp {
  Operand src;
  Operand dst;
} AddOp;

typedef struct SubOp {
  Operand src;
  Operand dst;
} SubOp;

typedef struct CmpOp {
  Operand src;
  Operand dst;
} CmpOp;

typedef struct ConditionalJumpOp {
  int offset;
} ConditionalJumpOp;

typedef union OpData {
  MovOp mov;
  UnknownOp unkn;
  AddOp add;
  SubOp sub;
  CmpOp cmp;
  ConditionalJumpOp cond_jmp;
} OpData;

typedef enum Op {
  MOV,
  ADD,
  SUB,
  CMP,
  JE,
  JL,
  JLE,
  JB,
  JBE,
  JP,
  JO,
  JS,
  JNE,
  JNL,
  JNLE,
  JNB,
  JNBE,
  JNP,
  JNO,
  JNS,
  LOOP,
  LOOPZ,
  LOOPNZ,
  JCXZ,
  UNKNOWN_OP,
} Op;

typedef struct Instruction {
  Op op_type;
  OpData op_data;
} Instruction;

Instruction parse_instr(unsigned char **ip);
void print_instr(Instruction *i);

#endif // _DECODER_H
