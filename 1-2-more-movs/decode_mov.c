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

typedef struct DirectAddressLoc {
  int addr;
} DirectAddressLoc;

typedef struct EffectiveAddressLoc {
  /** first register of the equation */
  Reg operand1;
  /** second register of the equation, nullable */
  Reg operand2;
  /** resolved value (from displacement), nullable */
  int operand3;
} EffectiveAddressLoc;

typedef struct RegisterLoc {
  Reg r;
} RegisterLoc;

typedef struct ImmediateLoc {
  int val;
} ImmediateLoc;

typedef enum LocationType {
  DIRECT_ADDR_LOC,
  EFFECTIVE_ADDR_LOC,
  REG_LOC,
  IMMEDIATE_LOC
} LocationType;

typedef union LocationData {
  DirectAddressLoc addr;
  EffectiveAddressLoc e_addr;
  RegisterLoc reg;
  ImmediateLoc imm;
} LocationData;

typedef struct Location {
  LocationType t;
  LocationData loc;
} Location;

typedef enum Mod {
  MEM_NO_DISP,
  MEM_DISP_8,
  MEM_DISP_16,
  REG,
} Mod;

typedef struct MovOp {
  Location src;
  Location dst;
} MovOp;

typedef struct UnknownOp {
} UnknownOp;

typedef union OpData {
  MovOp mov;
  UnknownOp unkn;
} OpData;

typedef enum Op {
  MOV,
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
 * Note: this function parses the bytes starting at **ip into an instruction
 * and will move the instruction pointer along as it does so.
 * When this function is done, the instruction pointer points at the first
 * unparsed byte.
 */
Instruction parse_mov_reg_rm(unsigned char **ip) {
  int W = **ip & 1;
  int D = (**ip >> 1) & 1;
  (*ip)++;

  Mod mod = parse_mode((**ip) >> 6);
  int rm = (**ip) & 0b111;
  Reg reg = parse_register(W, ((**ip) >> 3 & 0b111));
  LocationData reg_loc = {.reg = reg};

  Location src;
  Location dst;

  LocationType l_type;
  LocationData l_data;

  if (mod != REG) {
    int operand3;

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
      EffectiveAddressLoc eal = {
          .operand1 = BX, .operand2 = SI, .operand3 = operand3};
      l_type = EFFECTIVE_ADDR_LOC;
      l_data.e_addr = eal;
    } else if (rm == 0b001) {
      EffectiveAddressLoc eal = {
          .operand1 = BX, .operand2 = DI, .operand3 = operand3};
      l_type = EFFECTIVE_ADDR_LOC;
      l_data.e_addr = eal;
    } else if (rm == 0b010) {
      EffectiveAddressLoc eal = {
          .operand1 = BP, .operand2 = SI, .operand3 = operand3};
      l_type = EFFECTIVE_ADDR_LOC;
      l_data.e_addr = eal;
    } else if (rm == 0b011) {
      EffectiveAddressLoc eal = {
          .operand1 = BP, .operand2 = DI, .operand3 = operand3};
      l_type = EFFECTIVE_ADDR_LOC;
      l_data.e_addr = eal;
    } else if (rm == 0b100) {
      EffectiveAddressLoc eal = {.operand1 = SI, .operand3 = operand3};
      l_type = EFFECTIVE_ADDR_LOC;
      l_data.e_addr = eal;
    } else if (rm == 0b101) {
      EffectiveAddressLoc eal = {.operand1 = DI, .operand3 = operand3};
      l_type = EFFECTIVE_ADDR_LOC;
      l_data.e_addr = eal;
    } else if (rm == 0b110) {
      /**
       * Corner case: if we are in MEM_NO_DISP mode, then this R/M value means
       * we need to read a direct address
       */
      if (mod == MEM_NO_DISP) {
        l_type = DIRECT_ADDR_LOC;
        (*ip)++;
        int lo = **ip;
        DirectAddressLoc dal;
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
        l_data.addr = dal;
      } else {
        EffectiveAddressLoc eal = {.operand1 = BP, .operand3 = operand3};
        l_type = EFFECTIVE_ADDR_LOC;
        l_data.e_addr = eal;
      }
    } else if (rm == 0b111) {
      EffectiveAddressLoc eal = {.operand1 = BX};
      l_data.e_addr = eal;
    }
  } else {
    RegisterLoc rl = {.r = parse_register(W, (**ip) & 0b111)};
    l_type = REG_LOC;
    l_data.reg = rl;
  }

  if (D) {
    dst.t = REG_LOC;
    dst.loc = reg_loc;
    src.t = l_type;
    src.loc = l_data;
  } else {
    dst.t = l_type;
    dst.loc = l_data;
    src.t = REG_LOC;
    src.loc = reg_loc;
  }

  MovOp mov = {.src = src, .dst = dst};
  OpData d = {.mov = mov};
  Instruction i = {.op_type = MOV, .op_data = d};
  (*ip)++;

  return i;
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
  LocationData dst_loc_data = {.reg = dst};
  Location dst_loc = {.t = REG_LOC, .loc = dst_loc_data};

  (*ip)++;
  int src_lo = **ip;
  int src_addr = src_lo;
  if (W) {
    /** dealing with a 16-bit number; need another byte */
    (*ip)++;
    int src_hi = **ip;
    src_addr = (src_hi << 8) | src_lo;
  }

  LocationData src_loc_data = {.imm = src_addr};
  Location src_loc = {.t = IMMEDIATE_LOC, .loc = src_loc_data};

  MovOp mov = {.src = src_loc, .dst = dst_loc};
  OpData op_data = {.mov = mov};

  Instruction i = {.op_type = MOV, .op_data = op_data};
  (*ip)++;

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

void print_loc(Location *l) {
  switch (l->t) {
  case DIRECT_ADDR_LOC:
    printf("%d", l->loc.addr.addr);
    break;
  case EFFECTIVE_ADDR_LOC:
    printf("[");
    print_reg(&l->loc.e_addr.operand1);
    if (l->loc.e_addr.operand2 != NO_REG) {
      printf(" + ");
      print_reg(&l->loc.e_addr.operand2);
    }

    if (l->loc.e_addr.operand3 != 0) {
      printf(" + %d", l->loc.e_addr.operand3);
    }
    printf("]");
    break;
  case REG_LOC:
    print_reg(&l->loc.reg.r);
    break;
  case IMMEDIATE_LOC:
    printf("%d", l->loc.imm.val);
    break;
  }
}

void print_instr(Instruction *i) {
  switch (i->op_type) {
  case MOV:
    printf("mov ");
    print_loc(&i->op_data.mov.dst);
    printf(", ");
    print_loc(&i->op_data.mov.src);
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
