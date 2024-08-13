#include <stdio.h>

typedef enum {
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
  UNKNOWN_REG
} reg;

typedef enum {
  MOV,
  UNKNOWN_OP,
} opcode;

typedef enum {
  MEM_NO_DISP,
  MEM_DISP_8,
  MEM_DISP_16,
  REG,
  UNKNOWN_MOD,
} mod;

typedef struct {
  opcode opcode;
  int D;
  int W;
  mod mod;
  reg src;
  reg dst;
} instruction ;

opcode parse_opcode(unsigned char b) {
  switch (b) {
  case 0x22:
    return MOV;

  default:
    return UNKNOWN_OP;
  }
}

mod parse_mode(unsigned char b) {
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
    return UNKNOWN_MOD;
  }
}

reg parse_register(int W, unsigned char b) {
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
    return UNKNOWN_REG;
  }
}

instruction parse_inst(const unsigned char buf[]) {
  int W = buf[0] & 1;
  int D = (buf[0] >> 1) & 1;
  opcode op = parse_opcode(buf[0] >> 2);

  reg src = parse_register(W, D ? (buf[1] & 7) : (buf[1] >> 3) & 7);
  reg dst = parse_register(W, D ? ((buf[1] >> 3) & 7) : buf[1] & 7);
  mod mod = parse_mode(buf[1] >> 6);

  instruction ret = {
      .opcode = op, .W = W, .D = D, .mod = mod, .dst = dst, .src = src};

  return ret;
}

void print_reg(reg *r) {
  switch (*r) {
  case AL:
    printf("AL");
    break;
  case AX:
    printf("AX");
    break;
  case CL:
    printf("CL");
    break;
  case CX:
    printf("CX");
    break;
  case DL:
    printf("DL");
    break;
  case DX:
    printf("DX");
    break;
  case BL:
    printf("BL");
    break;
  case BX:
    printf("BX");
    break;
  case AH:
    printf("AH");
    break;
  case SP:
    printf("SP");
    break;
  case CH:
    printf("CH");
    break;
  case BP:
    printf("BP");
    break;
  case DH:
    printf("DH");
    break;
  case SI:
    printf("SI");
    break;
  case BH:
    printf("BH");
    break;
  case DI:
    printf("DI");
    break;
  case UNKNOWN_REG:
    printf("UNK");
    break;
  }
}

void print_instruction(instruction *inst) {
  switch (inst->opcode) {
  case MOV:
    printf("MOV ");
    break;
  case UNKNOWN_OP:
    printf("UNK ");
    break;
  }

  print_reg(&inst->dst);
  printf(", ");
  print_reg(&inst->src);
  printf("\n");
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

  for (int i = 0; i < len; i += 2) {
    printf("%d: %x %x > ", i, buf[i], buf[i + 1]);
    unsigned char *ptr = buf + i;
    instruction i = parse_inst(ptr);
    print_instruction(&i);
  }

  return 0;
}
