#include "../decoder/decoder.h"

#include <stdint.h>
#include <stdio.h>

typedef struct VM {
  unsigned char *memory;
  int memory_len;
  unsigned char *ip;
  unsigned char *end;
  uint16_t registers[8];
} VM;

VM new_vm(unsigned char *memory, int memory_len) {
  VM vm = {.memory = memory,
           .memory_len = memory_len,
           .ip = memory,
           .end = memory + memory_len,
           .registers = {0, 0, 0, 0, 0, 0, 0, 0}};

  return vm;
}

size_t reg_to_index(Reg r) {
  switch (r) {
  case AX:
    return 0;
  case BX:
    return 1;
  case CX:
    return 2;
  case DX:
    return 3;
  case SP:
    return 4;
  case BP:
    return 5;
  case SI:
    return 6;
  case DI:
    return 7;
  default:
    return 0;
  }
}

void print_reg_by_idx(size_t index) {
  if (index == 0)
    printf("ax");
  if (index == 1)
    printf("bx");
  if (index == 2)
    printf("cx");
  if (index == 3)
    printf("dx");
  if (index == 4)
    printf("sp");
  if (index == 5)
    printf("bp");
  if (index == 6)
    printf("si");
  if (index == 7)
    printf("di");
}

uint8_t read_reg(VM *vm, Reg src) { return vm->registers[reg_to_index(src)]; }

void write_reg(VM *vm, Reg dst, uint16_t value) {
  uint16_t current_value = read_reg(vm, dst);
  size_t i = reg_to_index(dst);
  print_reg_by_idx(i);
  printf(": %d -> %d\n", current_value, value);
  vm->registers[reg_to_index(dst)] = value;
}

void tick(VM *vm) {
  Instruction i = parse_instr(&vm->ip);
  switch (i.op_type) {
  case MOV: {
    MovOp m = i.op_data.mov;
    if (m.dst.t == REGISTER) {
      uint16_t value = 0;
      if (m.src.t == IMMEDIATE) {
        value = m.src.operand.imm.val;
      } else if (m.src.t == REGISTER) {
        value = read_reg(vm, m.src.operand.reg.r);
      }
      write_reg(vm, m.dst.operand.reg.r, value);
    }
  } break;

  default:
    return;
  }
}

void run(VM *vm) {
  while (vm->ip < vm->end) {
    tick(vm);
  }
}

void dump_registers(VM *vm) {
  for (int i = 0; i < 8; i++) {
    print_reg_by_idx(i);
    printf(": %d\n", vm->registers[i]);
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <input_binary>\n", argv[0]);
    return 1;
  }

  FILE *f = fopen(argv[1], "r");
  if (f == NULL) {
    fprintf(stderr, "unable to open file %s\n", argv[1]);
    return 1;
  }

  unsigned char buf[1000];
  int buf_len = 0;

  int c;
  while ((c = getc(f)) != EOF) {
    buf[buf_len] = c;
    buf_len++;
  }

  VM vm = new_vm(buf, buf_len);
  run(&vm);
  dump_registers(&vm);
}
