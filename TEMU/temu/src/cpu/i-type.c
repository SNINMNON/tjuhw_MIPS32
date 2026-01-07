#include "helper.h"
#include "monitor.h"
#include "reg.h"
#include "special.h"

extern uint32_t instr;
extern char assembly[80];

/* decode I-type instrucion with unsigned immediate */
static void decode_imm_type(uint32_t instr) {
	// rs
	op_src1->type = OP_TYPE_REG;
	op_src1->reg = (instr & RS_MASK) >> (RT_SIZE + IMM_SIZE);
	op_src1->val = reg_w(op_src1->reg);
	// rt
	op_dest->type = OP_TYPE_REG;
	op_dest->reg = (instr & RT_MASK) >> (IMM_SIZE);
	op_dest->val = reg_w(op_dest->reg);
	// imm
	op_src2->type = OP_TYPE_IMM;
	op_src2->imm = instr & IMM_MASK;
	op_src2->val = op_src2->imm;
}

make_helper(addi) {
    decode_imm_type(instr);

    int32_t a = (int32_t)op_src1->val;
    int32_t b = (int32_t)(int16_t)op_src2->imm;  // sign-extend imm16
    int32_t s = a + b;

    // overflow if a and b same sign, but s different sign
    if (((a ^ b) >= 0) && ((a ^ s) < 0)) {
        assert(0);
    } else {
		reg_w(op_dest->reg) = (uint32_t)s;
	}
	sprintf(assembly, "addi   %s,   %s,   0x%04x", REG_NAME(op_dest->reg), REG_NAME(op_src1->reg), op_src2->imm);
}

make_helper(addiu) {
	decode_imm_type(instr);
	reg_w(op_dest->reg) = op_src1->val + (int32_t)(int16_t)op_src2->imm;
	sprintf(assembly, "addiu   %s,   %s,   0x%04x", REG_NAME(op_dest->reg), REG_NAME(op_src1->reg), op_src2->imm);
}

make_helper(andi) {
	decode_imm_type(instr);
	reg_w(op_dest->reg) = op_src1->val & op_src2->val;
	sprintf(assembly, "andi   %s,   %s,   0x%04x", REG_NAME(op_dest->reg), REG_NAME(op_src1->reg), op_src2->imm);
}

make_helper(lui) {
	decode_imm_type(instr);
	reg_w(op_dest->reg) = (op_src2->val << 16);
	sprintf(assembly, "lui   %s,   0x%04x", REG_NAME(op_dest->reg), op_src2->imm);
}

make_helper(ori) {
	decode_imm_type(instr);
	reg_w(op_dest->reg) = op_src1->val | op_src2->val;
	sprintf(assembly, "ori   %s,   %s,   0x%04x", REG_NAME(op_dest->reg), REG_NAME(op_src1->reg), op_src2->imm);
}

make_helper(beq) {
	decode_imm_type(instr);
	if (op_src1->val == op_dest->val) {
		cpu.pc += (int32_t)(int16_t)op_src2->val * 4;
	}
	sprintf(assembly, "beq   %s,   %s,   0x%04x", REG_NAME(op_src1->reg), REG_NAME(op_dest->reg), op_src2->imm);
}

make_helper(bne) {
	decode_imm_type(instr);
	if (op_src1->val != op_dest->val) {
		cpu.pc += (int32_t)(int16_t)op_src2->imm * 4;
	}
	sprintf(assembly, "bne   %s,   %s,   0x%04x", REG_NAME(op_src1->reg), REG_NAME(op_dest->reg), op_src2->imm);
}

make_helper(lb) {
	decode_imm_type(instr);
	uint32_t addr = op_src1->val + (int32_t)(int16_t)op_src2->imm;
	addr = addr & 0x1fffffff;
	reg_w(op_dest->reg) = (int32_t)(int8_t)mem_read(addr, 1);
	sprintf(assembly, "lb   %s,   0x%04x(%s)", REG_NAME(op_dest->reg), op_src2->imm, REG_NAME(op_src1->reg));
}

make_helper(lw) {
	decode_imm_type(instr);
	uint32_t addr = op_src1->val + (int32_t)(int16_t)op_src2->imm;
	addr = addr & 0x1fffffff;
	reg_w(op_dest->reg) = mem_read(addr, 4);
	sprintf(assembly, "lw   %s,   0x%04x(%s)", REG_NAME(op_dest->reg), op_src2->imm, REG_NAME(op_src1->reg));
}

make_helper(sb) {
	decode_imm_type(instr);
	uint32_t addr = op_src1->val + (int32_t)(int16_t)op_src2->imm;
	addr = addr & 0x1fffffff;
	mem_write(addr, 1, reg_w(op_dest->reg) & 0xFF);
	sprintf(assembly, "sb   %s,   0x%04x(%s)", REG_NAME(op_dest->reg), op_src2->imm, REG_NAME(op_src1->reg));
}

make_helper(sw) {
	decode_imm_type(instr);
	uint32_t addr = op_src1->val + (int32_t)(int16_t)op_src2->imm;
	addr = addr & 0x1fffffff;
	mem_write(addr, 4, reg_w(op_dest->reg));
	sprintf(assembly, "sw   %s,   0x%04x(%s)", REG_NAME(op_dest->reg), op_src2->imm, REG_NAME(op_src1->reg));
}
