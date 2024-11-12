#include "../decoder/decoder.h"

#include <stdint.h>
#include <stdio.h>

typedef struct VM {
  unsigned char *memory;
  int memory_len;
  unsigned char *ip;
  unsigned char *end;
  uint16_t registers[8];
  uint16_t flags;
} VM;

VM new_vm(unsigned char *memory, int memory_len) {
  VM vm = {.memory = memory,
           .memory_len = memory_len,
           .ip = memory,
           .end = memory + memory_len,
           .registers = {0, 0, 0, 0, 0, 0, 0, 0},
           .flags = 0};

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

uint16_t read_reg(VM *vm, Reg src) { return vm->registers[reg_to_index(src)]; }

void write_reg(VM *vm, Reg dst, uint16_t value) {
  uint16_t current_value = read_reg(vm, dst);
  size_t i = reg_to_index(dst);
  print_reg_by_idx(i);
  printf(": %d -> %d\n", current_value, value);
  vm->registers[reg_to_index(dst)] = value;
}

void dump_registers(VM *vm) {
  for (int i = 0; i < 8; i++) {
    print_reg_by_idx(i);
    printf(": %x (%d); ", vm->registers[i], vm->registers[i]);
  }
  printf("\n");
  printf("ip: %p\n", vm->ip);
}

void resolve_operands(VM *vm, Operand *operands, uint16_t *resolved_operands,
                      size_t n_operands) {
  for (int i = 0; i < n_operands; i++) {
    Operand op = operands[i];
    if (op.t == IMMEDIATE) {
      resolved_operands[i] = op.operand.imm.val;
    } else if (op.t == REGISTER) {
      resolved_operands[i] = read_reg(vm, op.operand.reg.r);
    }
  }
}

void update_flags(VM *vm, uint16_t arithm_result) {
  if (arithm_result == 0) {
    vm->flags |= (1 << 3);
  } else {
    vm->flags &= ~(1 << 3);
  }

  // highest bit set? then set SF
  if (arithm_result & (1 << 15)) {
    vm->flags |= (1 << 4);
  } else {
    vm->flags &= ~(1 << 4);
  }
}

void dump_flags(VM *vm) {
  printf("flags: \nSF: %d, ZF: %d\n", (vm->flags >> 4) & 1,
         (vm->flags >> 3) & 1);
}

void tick(VM *vm) {
  Instruction i = parse_instr(&vm->ip);
  print_instr(&i);
  printf(" :: ");
  switch (i.op_type) {
  case MOV: {
    MovOp m = i.op_data.mov;
    if (m.dst.t == REGISTER) {
      uint16_t resolved_values[1] = {0};
      resolve_operands(vm, &m.src, resolved_values, 1);
      write_reg(vm, m.dst.operand.reg.r, resolved_values[0]);
    }
  } break;

  case ADD: {
    AddOp a = i.op_data.add;
    if (a.dst.t == REGISTER && (a.src.t == REGISTER || a.src.t == IMMEDIATE)) {
      Operand operands[2] = {a.dst, a.src};
      uint16_t resolved_values[2] = {0};
      resolve_operands(vm, operands, resolved_values, 2);

      uint16_t result = resolved_values[0] + resolved_values[1];
      write_reg(vm, a.dst.operand.reg.r, result);
      update_flags(vm, result);
    }
  } break;

  case SUB: {
    SubOp s = i.op_data.sub;
    if (s.dst.t == REGISTER && (s.src.t == REGISTER || s.src.t == IMMEDIATE)) {
      Operand operands[2] = {s.dst, s.src};
      uint16_t resolved_values[2] = {0, 0};
      resolve_operands(vm, operands, resolved_values, 2);

      uint16_t result = resolved_values[0] - resolved_values[1];
      write_reg(vm, s.dst.operand.reg.r, result);
      update_flags(vm, result);
    }
  } break;

  case CMP: {
    CmpOp c = i.op_data.cmp;
    if (c.dst.t == REGISTER && (c.src.t == REGISTER || c.src.t == IMMEDIATE)) {
      Operand operands[2] = {c.dst, c.src};
      uint16_t resolved_values[2] = {0, 0};
      resolve_operands(vm, operands, resolved_values, 2);

      uint16_t result = resolved_values[0] - resolved_values[1];
      update_flags(vm, result);
    }
  } break;

  case JNE: {
    int zf = vm->flags >> 3 & 1;
    if (zf == 0) {
      ConditionalJumpOp c = i.op_data.cond_jmp;
      (vm->ip) += c.offset;
    }
  }

  default:
    return;
  }
}

void run(VM *vm) {
  while (vm->ip < vm->end) {
    tick(vm);
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
  dump_flags(&vm);
}