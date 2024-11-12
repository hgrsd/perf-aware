#include "decoder.h"
#include <stdint.h>
#include <stdio.h>

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
      EffectiveAddr eal = {
          .operand1 = SI, .operand2 = NO_REG, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    } else if (rm == 0b101) {
      EffectiveAddr eal = {
          .operand1 = DI, .operand2 = NO_REG, .operand3 = operand3};
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
        EffectiveAddr eal = {
            .operand1 = BP, .operand2 = NO_REG, .operand3 = operand3};
        op_type = EFFECTIVE_ADDR;
        op_data.e_addr = eal;
      }
    } else if (rm == 0b111) {
      EffectiveAddr eal = {
          .operand1 = BX, .operand2 = NO_REG, .operand3 = operand3};
      op_type = EFFECTIVE_ADDR;
      op_data.e_addr = eal;
    }
  } else {
    Register rl = {.r = parse_register(W, (**ip) & 0b111)};
    op_type = REGISTER;
    op_data.reg = rl;
  }

  Operand o = {.t = op_type, .operand = op_data};

  (*ip)++;
  return o;
}

Operand parse_immediate(int W, unsigned char **ip) {
  int src_lo = **ip;
  int src_addr = src_lo;
  if (W) {
    /** dealing with a 16-bit number; need another byte */
    (*ip)++;
    int src_hi = **ip;
    src_addr = (src_hi << 8) | src_lo;
  }

  OperandData op_data = {.imm = src_addr};
  Operand op = {.t = IMMEDIATE, .operand = op_data};

  (*ip)++;
  return op;
}

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
}

Instruction parse_mov_im_reg(unsigned char **ip) {
  int W = (**ip) >> 3 & 1;
  Reg dst = parse_register(W, (**ip & 0b111));
  OperandData dst_operand_data = {.reg = dst};
  Operand dst_operand = {.t = REGISTER, .operand = dst_operand_data};

  (*ip)++;

  Operand op_imm = parse_immediate(W, ip);
  OpData op = {.mov = {.dst = dst_operand, .src = op_imm}};
  Instruction i = {.op_type = MOV, .op_data = op};

  return i;
}

Instruction parse_mov_im_rm(unsigned char **ip) {
  int W = (**ip) & 1;
  Operand op_dst = parse_rm_operand(W, ip);
  Operand op_imm = parse_immediate(W, ip);
  MovOp mov = {.dst = op_dst, .src = op_imm};
  OpData op = {.mov = mov};
  Instruction i = {.op_type = MOV, .op_data = op};
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

Instruction parse_add_reg_rm(unsigned char **ip) {
  Operand ops[2];
  parse_operands_reg_rm(ip, ops);
  AddOp add = {.dst = ops[0], .src = ops[1]};
  OpData op = {.add = add};
  Instruction i = {.op_type = ADD, .op_data = op};
  return i;
}

Instruction parse_add_im_rm(unsigned char **ip) {
  int S = ((**ip) >> 1) & 1;
  int W = **ip & 1;
  (*ip)++;
  Operand op_dst = parse_rm_operand(W, ip);
  Operand op_imm = parse_immediate(S == 0 && W == 1 ? 1 : 0, ip);
  AddOp add = {.dst = op_dst, .src = op_imm};
  OpData op = {.add = add};
  Instruction i = {.op_type = ADD, .op_data = op};
  return i;
}

Instruction parse_add_im_to_acc(unsigned char **ip) {
  int W = **ip & 1;

  Register reg = {.r = W ? AX : AL};
  Operand dst = {.t = REGISTER, .operand = {.reg = reg}};
  (*ip)++;
  Operand src = parse_immediate(W, ip);
  AddOp add = {.dst = dst, .src = src};
  OpData op = {.add = add};
  Instruction i = {.op_type = ADD, .op_data = op};
  return i;
}

Instruction parse_sub_reg_rm(unsigned char **ip) {
  Operand ops[2];
  parse_operands_reg_rm(ip, ops);
  SubOp sub = {.dst = ops[0], .src = ops[1]};
  OpData op = {.sub = sub};
  Instruction i = {.op_type = SUB, .op_data = op};
  return i;
}

Instruction parse_sub_im_rm(unsigned char **ip) {
  int S = ((**ip) >> 1) & 1;
  int W = **ip & 1;
  (*ip)++;
  Operand op_dst = parse_rm_operand(W, ip);
  Operand op_imm = parse_immediate(S == 0 && W == 1 ? 1 : 0, ip);
  SubOp sub = {.dst = op_dst, .src = op_imm};
  OpData op = {.sub = sub};
  Instruction i = {.op_type = SUB, .op_data = op};
  return i;
}

Instruction parse_sub_im_to_acc(unsigned char **ip) {
  int W = **ip & 1;

  Register reg = {.r = W ? AX : AL};
  Operand dst = {.t = REGISTER, .operand = {.reg = reg}};
  (*ip)++;
  Operand src = parse_immediate(W, ip);
  SubOp sub = {.dst = dst, .src = src};
  OpData op = {.sub = sub};
  Instruction i = {.op_type = SUB, .op_data = op};
  return i;
}

Instruction parse_cmp_reg_rm(unsigned char **ip) {
  Operand ops[2];
  parse_operands_reg_rm(ip, ops);
  CmpOp cmp = {.dst = ops[0], .src = ops[1]};
  OpData op = {.cmp = cmp};
  Instruction i = {.op_type = CMP, .op_data = op};
  return i;
}

Instruction parse_cmp_im_rm(unsigned char **ip) {
  int S = ((**ip) >> 1) & 1;
  int W = **ip & 1;
  (*ip)++;
  Operand op_dst = parse_rm_operand(W, ip);
  Operand op_imm = parse_immediate(S == 0 && W == 1 ? 1 : 0, ip);
  CmpOp cmp = {.dst = op_dst, .src = op_imm};
  OpData op = {.cmp = cmp};
  Instruction i = {.op_type = CMP, .op_data = op};
  return i;
}

Instruction parse_cmp_im_to_acc(unsigned char **ip) {
  int W = **ip & 1;

  Register reg = {.r = W ? AX : AL};
  Operand dst = {.t = REGISTER, .operand = {.reg = reg}};
  (*ip)++;
  Operand src = parse_immediate(W, ip);
  CmpOp cmp = {.dst = dst, .src = src};
  OpData op = {.cmp = cmp};
  Instruction i = {.op_type = CMP, .op_data = op};
  return i;
}

int offset_ip_inc8(int n) {
  if (n & 0x80) {
    return n | ~0xFF;
  }
  return n;
}

Instruction parse_instr(unsigned char **ip) {
  int b0 = (*ip)[0];
  int b1 = (*ip)[1];

  /** MOV */
  if (b0 >> 2 == 0b100010) {
    return parse_mov_reg_rm(ip);
  }

  if (b0 >> 1 == 0b1100011 && ((b1 >> 3) & 0b111) == 0b000) {
    return parse_mov_im_rm(ip);
  }

  if (b0 >> 4 == 0b1011) {
    return parse_mov_im_reg(ip);
  }
  /** MOV END */

  /** ADD */
  if (b0 >> 2 == 0b000000) {
    return parse_add_reg_rm(ip);
  }

  if (b0 >> 2 == 0b100000 && ((b1 >> 3) & 0b111) == 0b000) {
    return parse_add_im_rm(ip);
  }

  if (b0 >> 1 == 0b0000010) {
    return parse_add_im_to_acc(ip);
  }
  /** ADD END */

  /** SUB */
  if (b0 >> 2 == 0b001010) {
    return parse_sub_reg_rm(ip);
  }

  if (b0 >> 2 == 0b100000 && ((b1 >> 3) & 0b111) == 0b101) {
    return parse_sub_im_rm(ip);
  }

  if (b0 >> 1 == 0b0010110) {
    return parse_sub_im_to_acc(ip);
  }
  /** SUB END */

  /** CMP */
  if (b0 >> 2 == 0b001110) {
    return parse_cmp_reg_rm(ip);
  }

  if (b0 >> 2 == 0b100000 && ((b1 >> 3) & 0b111) == 0b111) {
    return parse_cmp_im_rm(ip);
  }

  if (b0 >> 1 == 0b0011110) {
    return parse_cmp_im_to_acc(ip);
  }
  /** CMP END */

  /** CONDITIONAL JUMPS */
  if (b0 == 0b01110100) {
    Instruction i = {.op_type = JE,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }
  if (b0 == 0b01111100) {
    Instruction i = {.op_type = JL,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01111110) {
    Instruction i = {.op_type = JLE,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01110010) {
    Instruction i = {.op_type = JB,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01110110) {
    Instruction i = {.op_type = JBE,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01111010) {
    Instruction i = {.op_type = JP,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01110000) {
    Instruction i = {.op_type = JO,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01111000) {
    Instruction i = {.op_type = JS,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01110101) {
    Instruction i = {.op_type = JNE,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01111101) {
    Instruction i = {.op_type = JNL,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01111111) {
    Instruction i = {.op_type = JNLE,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01110011) {
    Instruction i = {.op_type = JNB,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01110111) {
    Instruction i = {.op_type = JNBE,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01111011) {
    Instruction i = {.op_type = JNP,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01110001) {
    Instruction i = {.op_type = JNO,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b01111001) {
    Instruction i = {.op_type = JNS,
                     .op_data = {.cond_jmp = {.offset = offset_ip_inc8(b1)}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b11100010) {
    Instruction i = {.op_type = LOOP, .op_data = {.cond_jmp = {.offset = b1}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b11100001) {
    Instruction i = {.op_type = LOOPZ, .op_data = {.cond_jmp = {.offset = b1}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b11100000) {
    Instruction i = {.op_type = LOOPNZ,
                     .op_data = {.cond_jmp = {.offset = b1}}};
    (*ip) += 2;
    return i;
  }

  if (b0 == 0b11100011) {
    Instruction i = {.op_type = JCXZ, .op_data = {.cond_jmp = {.offset = b1}}};
    (*ip) += 2;
    return i;
  }
  /** CONDITIONAL JUMPS END */

  (*ip)++;
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

void print_operand(Operand *o) {
  switch (o->t) {
  case DIRECT_ADDR:
    printf("%d", o->operand.addr.addr);
    break;
  case EFFECTIVE_ADDR:
    printf("[");
    print_reg(&o->operand.e_addr.operand1);
    if (o->operand.e_addr.operand2 != NO_REG) {
      printf(" + ");
      print_reg(&o->operand.e_addr.operand2);
    }

    if (o->operand.e_addr.operand3 != -1) {
      printf(" + %d", o->operand.e_addr.operand3);
    }
    printf("]");
    break;
  case REGISTER:
    print_reg(&o->operand.reg.r);
    break;
  case IMMEDIATE:
    printf("%d", o->operand.imm.val);
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
  case SUB:
    printf("sub ");
    print_operand(&i->op_data.mov.dst);
    printf(", ");
    print_operand(&i->op_data.mov.src);
    break;
  case CMP:
    printf("cmp ");
    print_operand(&i->op_data.mov.dst);
    printf(", ");
    print_operand(&i->op_data.mov.src);
    break;
  case JE:
    printf("je %d", i->op_data.cond_jmp.offset);
    break;
  case JL:
    printf("jl %d", i->op_data.cond_jmp.offset);
    break;
  case JLE:
    printf("jle %d", i->op_data.cond_jmp.offset);
    break;
  case JB:
    printf("jle %d", i->op_data.cond_jmp.offset);
    break;
  case JBE:
    printf("jbe %d", i->op_data.cond_jmp.offset);
    break;
  case JP:
    printf("jp %d", i->op_data.cond_jmp.offset);
    break;
  case JO:
    printf("jo %d", i->op_data.cond_jmp.offset);
    break;
  case JS:
    printf("js %d", i->op_data.cond_jmp.offset);
    break;
  case JNE:
    printf("jne %d", i->op_data.cond_jmp.offset);
    break;
  case JNL:
    printf("jne %d", i->op_data.cond_jmp.offset);
    break;
  case JNLE:
    printf("jne %d", i->op_data.cond_jmp.offset);
    break;
  case JNB:
    printf("jne %d", i->op_data.cond_jmp.offset);
    break;
  case JNBE:
    printf("jne %d", i->op_data.cond_jmp.offset);
    break;
  case JNP:
    printf("jne %d", i->op_data.cond_jmp.offset);
    break;
  case JNO:
    printf("jne %d", i->op_data.cond_jmp.offset);
    break;
  case JNS:
    printf("jns %d", i->op_data.cond_jmp.offset);
    break;
    break;
  case LOOP:
    printf("loop %d", i->op_data.cond_jmp.offset);
    break;
    break;
  case LOOPZ:
    printf("loopz %d", i->op_data.cond_jmp.offset);
    break;
    break;
  case LOOPNZ:
    printf("loopnz %d", i->op_data.cond_jmp.offset);
    break;
    break;
  case JCXZ:
    printf("jxcz %d", i->op_data.cond_jmp.offset);
    break;
    break;
  case UNKNOWN_OP:
    printf("UNKN");
    break;
  }
}
