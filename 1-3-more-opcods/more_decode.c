#include <stdio.h>

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

typedef union OpData {
  MovOp mov;
  UnknownOp unkn;
  AddOp add;
} OpData;

typedef enum Op {
  MOV,
  ADD,
  UNKNOWN_OP,
} Op;

typedef struct Instruction {
  Op op_type;
  OpData op_data;
} Instruction;

/**
 * Parse a byte into a mode.
 * This function expects the byte to have been shifted so that only its
 * first two bits are set.
 */
Mod parse_mode(unsigned char b) {
  switch (b) {
  case 0b00:
    return MEM_NO_DISP;
  case 0b01:
    return MEM_DISP_8;
  case 0b10:
    return MEM_DISP_16;
  case 0b11:
    return REG;
  default:
    return REG;
  }
}

/**
 * Parse a byte into a register based on the W flag.
 * This function expects the byte to have been shifted so that only its
 * first three bits are set.
 */
Reg parse_register(int W, unsigned char b) {
  switch (b) {
  case 0b000:
    return W ? AX : AL;
  case 0b001:
    return W ? CX : CL;
  case 0b010:
    return W ? DX : DL;
  case 0b011:
    return W ? BX : BL;
  case 0b100:
    return W ? SP : AH;
  case 0b101:
    return W ? BP : CH;
  case 0b110:
    return W ? SI : DH;
  case 0b111:
    return W ? DI : BH;
  default:
    return NO_REG;
  }
}

/**
 * Parse an operand based on the MOD and RM fields, in a REG/RM type operation.
 * This can return a register-based or memory-based operand.
 */
Operand parse_rm_operand(int W, unsigned char **ip) {
  Mod mod = parse_mode((**ip) >> 6);
  OperandType op_type;
  OperandData op_data;
  int rm = (**ip) & 0b111;

  if (mod != REG) {
    int operand3 = -1;

    if (mod == MEM_DISP_8) {
      /** 8 bit displacement: need to read an extra byte */
      (*ip)++;
      operand3 = **ip;
    } else if (mod == MEM_DISP_16) {
      /** 16 bit displacement: need to read two extra bytes */
      (*ip)++;
      int operand3_lo = **ip;
      (*ip)++;
      int operand3_hi = **ip;
      operand3 = (operand3_hi << 8) | operand3_lo;
    }

    if (rm == 0b000) {
      EffectiveAddr eal = {
          .operand1 = BX, .operand2 = SI, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    } else if (rm == 0b001) {
      EffectiveAddr eal = {
          .operand1 = BX, .operand2 = DI, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    } else if (rm == 0b010) {
      EffectiveAddr eal = {
          .operand1 = BP, .operand2 = SI, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    } else if (rm == 0b011) {
      EffectiveAddr eal = {
          .operand1 = BP, .operand2 = DI, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    } else if (rm == 0b100) {
      EffectiveAddr eal = {.operand1 = SI, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    } else if (rm == 0b101) {
      EffectiveAddr eal = {.operand1 = DI, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    } else if (rm == 0b110) {
      /**
       * Corner case: if we are in MEM_NO_DISP mode, then this R/M value means
       * we need to read a direct address
       */
      if (mod == MEM_NO_DISP) {
        op_type = DIRECT_ADDR;
        (*ip)++;
        int lo = **ip;
        DirectAddr dal;
        if (W) {
          /**
           * if W is set, we need to read another byte (DISP high) to get the 16
           * bit value
           */
          (*ip)++;
          int hi = **ip;
          int combined = (hi << 8) | lo;
          dal.addr = combined;
        } else {
          dal.addr = lo;
        }
        op_data.addr = dal;
      } else {
        EffectiveAddr eal = {.operand1 = BP, .operand3 = operand3};
        op_type = EFFECTIVE_ADDR;
        op_data.e_addr = eal;
      }
    } else if (rm == 0b111) {
      EffectiveAddr eal = {.operand1 = BX};
      op_data.e_addr = eal;
    }
  } else {
    Register rl = {.r = parse_register(W, (**ip) & 0b111)};
    op_type = REGISTER;
    op_data.reg = rl;
  }

  Operand o = {.t = op_type, .operand = op_data};
  return o;
}

/**
 * Note: this function parses the bytes starting at **ip into operands,
 * placing these in the provided ops array. ops[0] will be set to the
 * destination, and ops[1] to the source.
 *
 * This function will move the instruction pointer along as it parses. It
 * When this function is done, the instruction pointer points at the next
 * unparsed byte.
 */
void parse_operands_reg_rm(unsigned char **ip, Operand ops[]) {
  int W = **ip & 1;
  int D = (**ip >> 1) & 1;
  (*ip)++;

  Reg reg = parse_register(W, ((**ip) >> 3 & 0b111));
  OperandData operand_from_reg = {.reg = reg};

  Operand operand_from_rm = parse_rm_operand(W, ip);

  Operand src;
  Operand dst;

  if (D) {
    dst.t = REGISTER;
    dst.operand = operand_from_reg;
    src = operand_from_rm;
  } else {
    dst = operand_from_rm;
    src.t = REGISTER;
    src.operand = operand_from_reg;
  }

  ops[0] = dst;
  ops[1] = src;

  (*ip)++;
}

/**
 * Note: this function parses the bytes starting at **ip into an instruction
 * and will move the instruction pointer along as it does so.
 * When this function is done, the instruction pointer points at the first
 * unparsed byte.
 */
Instruction parse_mov_im_reg(unsigned char **ip) {
  int W = (**ip) >> 3 & 1;
  Reg dst = parse_register(W, (**ip & 0b111));
  OperandData dst_operand_data = {.reg = dst};
  Operand dst_operand = {.t = REGISTER, .operand = dst_operand_data};

  (*ip)++;
  int src_lo = **ip;
  int src_addr = src_lo;
  if (W) {
    /** dealing with a 16-bit number; need another byte */
    (*ip)++;
    int src_hi = **ip;
    src_addr = (src_hi << 8) | src_lo;
  }

  OperandData src_operand_data = {.imm = src_addr};
  Operand src_operand = {.t = IMMEDIATE, .operand = src_operand_data};

  MovOp mov = {.src = src_operand, .dst = dst_operand};
  OpData op_data = {.mov = mov};

  Instruction i = {.op_type = MOV, .op_data = op_data};
  (*ip)++;

  return i;
}

Instruction parse_mov_reg_rm(unsigned char **ip) {
  Operand ops[2];
  parse_operands_reg_rm(ip, ops);
  MovOp mov = {.dst = ops[0], .src = ops[1]};
  OpData op = {.mov = mov};
  Instruction i = {.op_type = MOV, .op_data = op};
  return i;
}

Instruction parse_instr(unsigned char **ip) {
  unsigned char byte = **ip;
  if (byte >> 2 == 0b100010) {
    return parse_mov_reg_rm(ip);
  }

  if (byte >> 4 == 0b1011) {
    return parse_mov_im_reg(ip);
  }

  // if (byte >> 2 == 0b000000) {
  //   return parse_add_reg_rm(ip);
  // }

  Instruction i = {.op_type = UNKNOWN_OP, .op_data = {.unkn = {}}};
  return i;
}

void print_reg(Reg *r) {
  switch (*r) {
  case NO_REG:
    break;
  case AL:
    printf("al");
    break;
  case AX:
    printf("ax");
    break;
  case CL:
    printf("cl");
    break;
  case CX:
    printf("cx");
    break;
  case DL:
    printf("dl");
    break;
  case DX:
    printf("dx");
    break;
  case BL:
    printf("bl");
    break;
  case BX:
    printf("bx");
    break;
  case AH:
    printf("ah");
    break;
  case SP:
    printf("sp");
    break;
  case CH:
    printf("ch");
    break;
  case BP:
    printf("bp");
    break;
  case DH:
    printf("dh");
    break;
  case SI:
    printf("si");
    break;
  case BH:
    printf("bh");
    break;
  case DI:
    printf("di");
    break;
  }
}

void print_operand(Operand *l) {
  switch (l->t) {
  case DIRECT_ADDR:
    printf("%d", l->operand.addr.addr);
    break;
  case EFFECTIVE_ADDR:
    printf("[");
    print_reg(&l->operand.e_addr.operand1);
    if (l->operand.e_addr.operand2 != NO_REG) {
      printf(" + ");
      print_reg(&l->operand.e_addr.operand2);
    }

    if (l->operand.e_addr.operand3 != -1) {
      printf(" + %d", l->operand.e_addr.operand3);
    }
    printf("]");
    break;
  case REGISTER:
    print_reg(&l->operand.reg.r);
    break;
  case IMMEDIATE:
    printf("%d", l->operand.imm.val);
    break;
  }
}

void print_instr(Instruction *i) {
  switch (i->op_type) {
  case MOV:
    printf("mov ");
    print_operand(&i->op_data.mov.dst);
    printf(", ");
    print_operand(&i->op_data.mov.src);
    break;
  case ADD:
    printf("add ");
    print_operand(&i->op_data.mov.dst);
    printf(", ");
    print_operand(&i->op_data.mov.src);
    break;
  case UNKNOWN_OP:
    printf("UNKN");
    break;
  }
}

int main(int argc, char **argv) {
  unsigned char buf[1000];
  int len = 0;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <filename>\n", argv[0]);
    return 1;
  }

  FILE *f = fopen(argv[1], "r");
  if (f == NULL) {
    fprintf(stderr, "unable to read file %s\n", argv[1]);
    return 1;
  }

  int c;
  while ((c = getc(f)) != EOF) {
    buf[len++] = c;
  }

  unsigned char *ip = buf;
  unsigned char *end = buf + len;

  while (ip < end) {
    Instruction i = parse_instr(&ip);
    print_instr(&i);
    printf("\n");
  }

  return 0;
}
