"""Observe executed Thumb memory instructions without Unicorn memory hooks.

Used by MAC probes because Unicorn 2.1.4 memory hooks can leave stale IT state.
Capstone decodes instructions; Unicorn still executes them unmodified. Unknown
memory instructions addressing the modeled peripheral windows fail closed.
"""
from __future__ import annotations
import capstone
from capstone import arm as arm
from unicorn import UC_MEM_READ, UC_MEM_WRITE
import unicorn.arm_const as uc_arm


class InstructionObserver:
    def __init__(self, machine):
        self.machine = machine
        self.decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)
        self.decoder.detail = True
        self.cache = {}
        self.aliases = {'sb': 'r9', 'sl': 'r10', 'fp': 'r11', 'ip': 'r12'}
        self.scalar = {
            arm.ARM_INS_LDR: (False, 4), arm.ARM_INS_LDRB: (False, 1),
            arm.ARM_INS_LDRH: (False, 2), arm.ARM_INS_LDRSB: (False, 1),
            arm.ARM_INS_LDRSH: (False, 2), arm.ARM_INS_STR: (True, 4),
            arm.ARM_INS_STRB: (True, 1), arm.ARM_INS_STRH: (True, 2),
        }
        self.unsupported_fp_multiple = {getattr(arm, name) for name in dir(arm)
                                        if name.startswith(('ARM_INS_VLDM', 'ARM_INS_VSTM'))}
        self.multiple = {}
        for name in ('LDM', 'LDMDA', 'LDMDB', 'LDMIB', 'STM', 'STMDA', 'STMDB', 'STMIB'):
            value = getattr(arm, 'ARM_INS_' + name, None)
            if value is not None:
                self.multiple[value] = name

    @staticmethod
    def peripheral(address):
        return 0x4D000000 <= address < 0x4D280000 or 0xE000E000 <= address < 0xE0010000

    def reg(self, instruction, register, cpu, pc):
        if register == 0:
            return 0
        name = instruction.reg_name(register)
        name = self.aliases.get(name, name)
        if name == 'pc':
            return (pc + 4) & ~3
        return cpu.reg_read(getattr(uc_arm, 'UC_ARM_REG_' + name.upper()))

    def address(self, instruction, operand, cpu, pc):
        mem = operand.mem
        index = self.reg(instruction, mem.index, cpu, pc)
        if operand.shift.type:
            if operand.shift.type != arm.ARM_SFT_LSL:
                raise RuntimeError('unsupported memory index shift')
            index <<= operand.shift.value
        if operand.subtracted:
            index = -index
        return (self.reg(instruction, mem.base, cpu, pc) + index * mem.scale + mem.disp) & 0xFFFFFFFF

    def access(self, cpu, address, width, write=False, value=0):
        if self.peripheral(address):
            if address + width - 1 >= 0x4D280000 and address < 0x4D280000:
                raise RuntimeError('peripheral access crosses modeled range')
            self.machine._memory(cpu, UC_MEM_WRITE if write else UC_MEM_READ,
                                 address, width, value & ((1 << (8 * width)) - 1), None)
            if write:
                callback = getattr(self.machine, 'write_response', None)
                if callback is not None:
                    callback(cpu, address, width, value)

    def __call__(self, cpu, pc, size, unused):
        if not 0 <= pc < 0x20000:
            raise RuntimeError(f'instruction observation outside declared ROM window: {pc:#x}')
        instruction = self.cache.get(pc)
        code = bytes(cpu.mem_read(pc, size))
        if instruction is None or bytes(instruction.bytes) != code:
            instruction = next(self.decoder.disasm(code, pc, count=1), None)
            if instruction is None or instruction.size != size:
                raise RuntimeError(f'cannot decode executed instruction at {pc:#x}')
            self.cache[pc] = instruction
        operands = instruction.operands
        if instruction.id in self.scalar:
            write, width = self.scalar[instruction.id]
            memory = next(op for op in operands if op.type == arm.ARM_OP_MEM)
            address = self.address(instruction, memory, cpu, pc)
            value = self.reg(instruction, operands[0].reg, cpu, pc) if write else 0
            self.access(cpu, address, width, write, value)
        elif instruction.id in (arm.ARM_INS_LDRD, arm.ARM_INS_STRD):
            write = instruction.id == arm.ARM_INS_STRD
            memory = next(op for op in operands if op.type == arm.ARM_OP_MEM)
            address = self.address(instruction, memory, cpu, pc)
            for index in range(2):
                value = self.reg(instruction, operands[index].reg, cpu, pc) if write else 0
                self.access(cpu, (address + index * 4) & 0xFFFFFFFF, 4, write, value)
        elif instruction.id in self.multiple:
            mode = self.multiple[instruction.id]
            registers = operands[1:]
            address = self.reg(instruction, operands[0].reg, cpu, pc)
            if mode.endswith('IB'):
                address += 4
            elif mode.endswith('DB'):
                address -= 4 * len(registers)
            elif mode.endswith('DA'):
                address -= 4 * (len(registers) - 1)
            for index, register in enumerate(registers):
                value = self.reg(instruction, register.reg, cpu, pc) if mode.startswith('STM') else 0
                self.access(cpu, (address + 4 * index) & 0xFFFFFFFF, 4, mode.startswith('STM'), value)
        elif instruction.id in (arm.ARM_INS_VLDR, arm.ARM_INS_VSTR):
            memory = next(op for op in operands if op.type == arm.ARM_OP_MEM)
            address = self.address(instruction, memory, cpu, pc)
            width = 8 if instruction.reg_name(operands[0].reg).startswith('d') else 4
            write = instruction.id == arm.ARM_INS_VSTR
            value = self.reg(instruction, operands[0].reg, cpu, pc) if write else 0
            self.access(cpu, address, width, write, value)
        else:
            if instruction.id in self.unsupported_fp_multiple:
                base = self.reg(instruction, operands[0].reg, cpu, pc)
                # Conservative span includes both incrementing and decrementing forms.
                span = 8 * len(operands)
                if any(self.peripheral((base + offset) & 0xFFFFFFFF) for offset in range(-span, span + 1)):
                    raise RuntimeError('unsupported peripheral floating-point multiple transfer')
            stack_ids = (arm.ARM_INS_PUSH, arm.ARM_INS_POP, arm.ARM_INS_VPUSH, arm.ARM_INS_VPOP)
            if instruction.id in stack_ids:
                stack = cpu.reg_read(uc_arm.UC_ARM_REG_SP)
                width = 8 if instruction.id in (arm.ARM_INS_VPUSH, arm.ARM_INS_VPOP) else 4
                start = stack - width * len(operands) if instruction.id in (arm.ARM_INS_PUSH, arm.ARM_INS_VPUSH) else stack
                if any(self.peripheral((start + width * i) & 0xFFFFFFFF) for i in range(len(operands))):
                    raise RuntimeError('unsupported peripheral stack access')
            for operand in operands:
                if operand.type == arm.ARM_OP_MEM:
                    address = self.address(instruction, operand, cpu, pc)
                    if self.peripheral(address):
                        raise RuntimeError(f'unsupported peripheral instruction {instruction.mnemonic} at {pc:#x}')
