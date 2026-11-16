#!/usr/bin/env python3
"""Conditional scalar equivalence checks using angr/PyVEX and SMT.

This is not a proof of the complete firmware or of the lifting engine. Inputs
are symbolic at declared AAPCS widths. Mutable RAM bytes read by a routine have shared symbolic initial values;
MMIO reads receive independent symbolic responses, with event guards preserved.
Initialized pointer cells retain their image values for these conditional checks.
All paths must terminate, cover the input domain, preserve the AAPCS saved
registers/SP, and agree on return values, final written RAM, and MMIO events.
Symbolic addresses fail closed except exhaustively enumerated immutable ROM
lookups (at most 16 addresses). Unsupported instructions and unfinished paths
fail closed.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import logging
import z3
import importlib.metadata
from pathlib import Path

logging.getLogger("angr.state_plugins.unicorn_engine").setLevel(logging.CRITICAL)
import angr
from angr import claripy as cp

ROOT = Path(__file__).resolve().parents[1]
REFERENCE_SHA256 = "15f3ce1c30b82e3f87c45974284ccfa68e10928fea0f09a1a46376ac04cd50d4"
STOP, STACK = 0x1F000, 0x20008000


def peripheral(address):
    return 0x4D000000 <= address < 0x4D280000 or 0xE000E000 <= address < 0xE0010000


def mutable(address):
    return (0x4D280000 <= address < 0x4D289298
            or 0x4D289900 <= address < 0x4D28D178
            or 0x4D2892C4 <= address < 0x4D2892C8
            or 0x4D2895AC <= address < 0x4D2895B4
            or 0x4D2895C0 <= address < 0x4D2895C4)


def ram(address):
    return 0x4D280000 <= address < 0x4D290000


class Proof:
    def __init__(self, elf):
        self.raw = (ROOT.parent / "phyb").read_bytes()
        assert hashlib.sha256(self.raw).hexdigest() == REFERENCE_SHA256
        self.elf = elf.read_bytes()
        self.reference = angr.Project(io.BytesIO(self.raw), main_opts={
            "backend": "blob", "arch": "ARMEL", "base_addr": 0, "entry_point": 1},
            auto_load_libs=False, use_sim_procedures=False)
        self.rebuilt = angr.Project(io.BytesIO(self.elf), auto_load_libs=False,
                                    use_sim_procedures=False)
        self.initial = {}
        self.queries = []
        self.context = "initialization"
        self.fixed_arguments = {}
        self.fixed_stack_arguments = {}
        self.args = [cp.BVS(f"argument_{i}", 32, explicit_name=True) for i in range(4)]
        self.saved = [cp.BVS(f"saved_r{i}", 32, explicit_name=True) for i in range(4, 12)]

    def solve(self, solver, label):
        result = solver.satisfiable()
        self.queries.append((self.context + ": " + label, result, solver.to_smt2()))
        return result

    def feasible(self, state, extra, label):
        solver = cp.Solver(timeout=10000)
        solver.add(list(state.solver.constraints) + list(extra))
        return self.solve(solver, label)

    def seed(self, state, address, length):
        seeded = state.globals["seeded"]
        additions = []
        for byte in range(address, address + length):
            if byte not in seeded:
                if byte not in self.initial:
                    self.initial[byte] = cp.BVS(f"initial_{byte:08x}", 8, explicit_name=True)
                state.memory.store(byte, self.initial[byte], inspect=False, disable_actions=True)
                additions.append(byte)
        state.globals["seeded"] = seeded.union(additions)

    @staticmethod
    def concrete(value, label):
        if isinstance(value, int):
            return value
        if value.symbolic:
            raise RuntimeError(f"symbolic {label}: {value}")
        return value.concrete_value

    def read_before(self, state):
        expression = state.inspect.attrs.mem_read_address
        length = self.concrete(state.inspect.attrs.mem_read_length, "read length")
        if not isinstance(expression, int) and expression.symbolic:
            values = state.solver.eval_upto(expression, 17)
            if not values or len(values) > 16 or not all(0 <= v and v + length <= 0x88D4 for v in values):
                raise RuntimeError(f"unbounded/non-ROM symbolic read: {expression}")
            if self.feasible(state, [cp.Not(cp.Or(*[expression == v for v in values]))], "ROM enumeration coverage"):
                raise RuntimeError("ROM lookup enumeration incomplete")
            state.globals["rom_lookup"] = (expression, length, tuple(values))
            state.inspect.attrs.mem_read_address = values[0]
            return
        address = self.concrete(expression, "read address")
        if mutable(address):
            self.seed(state, address, length)
        elif peripheral(address):
            # The actual read result is replaced after the memory load.
            state.memory.store(address, bytes(length), inspect=False, disable_actions=True)
        elif not (0 <= address < 0x88D4 or ram(address)
                  or 0x20000000 <= address < 0x20010000):
            raise RuntimeError(f"unmodeled read {address:#x}")

    @staticmethod
    def guard(condition):
        return cp.true() if condition is None else condition

    def read_after(self, state):
        lookup = state.globals.get("rom_lookup")
        if lookup is not None:
            expression, length, values = lookup
            result = None
            for value in values:
                data = state.project.loader.memory.load(value, length)
                loaded = cp.BVV(int.from_bytes(data, "little"), length * 8)
                result = loaded if result is None else cp.If(expression == value, loaded, result)
            guard = self.guard(state.inspect.attrs.mem_read_condition)
            state.inspect.attrs.mem_read_expr = cp.If(guard, result, state.inspect.attrs.mem_read_expr)
            state.globals["rom_lookup"] = None
            return
        address = self.concrete(state.inspect.attrs.mem_read_address, "read address")
        if peripheral(address):
            length = self.concrete(state.inspect.attrs.mem_read_length, "read length")
            counts = dict(state.globals["read_counts"])
            ordinal = counts.get(address, cp.BVV(0, 32))
            guard = self.guard(state.inspect.attrs.mem_read_condition)
            if self.feasible(state, [guard, cp.UGE(ordinal, 32)], "MMIO read bound"):
                raise RuntimeError("MMIO read count exceeds symbolic bound")
            indices = state.solver.eval_upto(ordinal, 33)
            if not indices or len(indices) > 32:
                raise RuntimeError("MMIO read ordinal enumeration incomplete")
            if self.feasible(state, [cp.Not(cp.Or(*[ordinal == i for i in indices]))], "MMIO ordinal coverage"):
                raise RuntimeError("MMIO ordinal enumeration does not cover path")
            response = None
            for index in indices:
                item = cp.BVS(f"response_{address:08x}_{length}_{index}", length * 8, explicit_name=True)
                response = item if response is None else cp.If(ordinal == index, item, response)
            counts[address] = ordinal + cp.If(guard, cp.BVV(1, 32), cp.BVV(0, 32))
            state.globals["read_counts"] = tuple(sorted(counts.items()))
            state.inspect.attrs.mem_read_expr = cp.If(guard, response, state.inspect.attrs.mem_read_expr)
            state.globals["events"] += (("read", address, length, response, guard),)

    def write_before(self, state):
        address = self.concrete(state.inspect.attrs.mem_write_address, "write address")
        length = self.concrete(state.inspect.attrs.mem_write_length, "write length")
        if ram(address) or peripheral(address):
            # Seed whole words, including bytes preserved by a narrow store.
            self.seed(state, address & ~3, ((address + length + 3) & ~3) - (address & ~3))
            state.globals["written"] = state.globals["written"].union(range(address, address + length))
            if peripheral(address):
                state.globals["events"] += (("write", address, length, state.inspect.attrs.mem_write_expr,
                                               self.guard(state.inspect.attrs.mem_write_condition)),)
        elif not 0x20000000 <= address < 0x20010000:
            raise RuntimeError(f"unmodeled write {address:#x}")

    def run(self, project, address, domains):
        state = project.factory.blank_state(addr=address | 1)
        if project is self.reference:
            state.memory.store(0x4D280000, self.raw[0x88E0:0x88E0 + 0x98BD])
        state.globals["seeded"] = frozenset()
        state.globals["written"] = frozenset()
        state.globals["events"] = ()
        state.globals["read_counts"] = ()
        state.regs.sp, state.regs.lr = STACK, STOP | 1
        for index, value in self.fixed_stack_arguments.items():
            state.memory.store(STACK + 4 * index, value.to_bytes(4, 'little'))
        # PyVEX ARM_COPY represents the initial NZCV flags in cc_dep1.
        state.regs.cc_op = state.regs.cc_dep2 = state.regs.cc_ndep = 0
        state.regs.cc_dep1 = cp.BVS("initial_nzcv", 32, explicit_name=True) & 0xF0000000
        for i, value in enumerate(self.args):
            if i in self.fixed_arguments:
                value = cp.BVV(self.fixed_arguments[i], 32)
                state.add_constraints(self.args[i] == value)
            setattr(state.regs, f"r{i}", value)
        for i, value in enumerate(self.saved, 4):
            setattr(state.regs, f"r{i}", value)
        for index, bits in enumerate(domains):
            if bits:
                state.add_constraints(self.args[index] <= (1 << bits) - 1)
        state.inspect.b("mem_read", when=angr.BP_BEFORE, action=self.read_before)
        state.inspect.b("mem_read", when=angr.BP_AFTER, action=self.read_after)
        state.inspect.b("mem_write", when=angr.BP_BEFORE, action=self.write_before)
        sim = project.factory.simgr(state)
        sim.stashes["returned"] = []
        steps = 0
        while sim.active and steps < 500:
            sim.move(from_stash="active", to_stash="returned", filter_func=lambda s: (s.addr & ~1) == STOP)
            if not sim.active:
                break
            sim.step()
            steps += 1
            if len(sim.active) > 128:
                raise RuntimeError("path count exceeds proof bound")
        if sim.active or sim.errored or sim.unconstrained or sim.deadended:
            raise RuntimeError(f"incomplete execution active={len(sim.active)} errors={sim.errored} "
                               f"unconstrained={len(sim.unconstrained)} deadended={len(sim.deadended)}")
        if not sim.stashes["returned"]:
            raise RuntimeError("no return paths")
        return sim.stashes["returned"], steps

    def check(self, name, original, domains=(), returns=True, fixed_arguments=None,
              fixed_stack_arguments=None):
        self.fixed_arguments = dict(fixed_arguments or {})
        self.fixed_stack_arguments = dict(fixed_stack_arguments or {})
        if any(i < 0 or i >= 4 or value < 0 or value > 0xFFFFFFFF
               for i, value in self.fixed_arguments.items()):
            raise ValueError("fixed arguments require R0-R3 and 32-bit values")
        if any(i < 0 or i >= 8 or value < 0 or value > 0xFFFFFFFF
               for i, value in self.fixed_stack_arguments.items()):
            raise ValueError("fixed stack arguments require word slots 0-7 and 32-bit values")
        self.context = name + (" " + repr(self.fixed_arguments) if self.fixed_arguments else "")
        if self.fixed_stack_arguments:
            self.context += " stack=" + repr(self.fixed_stack_arguments)
        symbol = self.rebuilt.loader.find_symbol(name)
        if symbol is None:
            return {"name": name, "status": "unresolved", "reason": "symbol absent"}
        try:
            left, ls = self.run(self.reference, original, domains)
            right, rs = self.run(self.rebuilt, symbol.rebased_addr, domains)
            domain = [self.args[i] <= (1 << n) - 1 for i, n in enumerate(domains) if n]
            domain.extend(self.args[i] == value for i, value in self.fixed_arguments.items())
            for paths in (left, right):
                solver = cp.Solver(timeout=10000)
                solver.add(domain)
                solver.add([cp.Not(cp.Or(*[cp.And(*s.solver.constraints) for s in paths]))])
                if self.solve(solver, "return-domain coverage"):
                    raise RuntimeError("return paths do not cover the declared input domain")
            pairs = 0
            for a in left:
                for b in right:
                    solver = cp.Solver(timeout=10000)
                    solver.add(a.solver.constraints + b.solver.constraints)
                    if not self.solve(solver, "paired path feasibility"):
                        continue
                    differences = [a.regs.sp != b.regs.sp, a.regs.sp != STACK]
                    if returns:
                        differences.append(a.regs.r0 != b.regs.r0)
                        if returns == "u64":
                            differences.append(a.regs.r1 != b.regs.r1)
                    for i, saved in enumerate(self.saved, 4):
                        differences.extend([getattr(a.regs, f"r{i}") != saved,
                                            getattr(b.regs, f"r{i}") != saved])
                    addresses = a.globals["written"].union(b.globals["written"])
                    for address in addresses:
                        if ram(address):
                            self.seed(a, address, 1)
                            self.seed(b, address, 1)
                            differences.append(a.memory.load(address, 1, inspect=False) !=
                                               b.memory.load(address, 1, inspect=False))
                    events_a, events_b = a.globals["events"], b.globals["events"]
                    slots = max(len(events_a), len(events_b))
                    def visible_trace(events):
                        output = [cp.BVV(0, 105) for _ in range(slots)]
                        position = cp.BVV(0, 32)
                        for kind, address, length, value, guard in events:
                            if value.size() > 64:
                                raise RuntimeError("MMIO event wider than 64 bits")
                            encoded = cp.Concat(cp.BVV(kind == "write", 1), cp.BVV(address, 32),
                                                cp.BVV(length, 8), cp.ZeroExt(64 - value.size(), value))
                            for slot in range(slots):
                                output[slot] = cp.If(cp.And(guard, position == slot), encoded, output[slot])
                            position = position + cp.If(guard, cp.BVV(1, 32), cp.BVV(0, 32))
                        return position, output
                    count_a, trace_a = visible_trace(events_a)
                    count_b, trace_b = visible_trace(events_b)
                    differences.append(count_a != count_b)
                    differences.extend(x != y for x, y in zip(trace_a, trace_b))
                    for expression in list(a.solver.constraints) + list(b.solver.constraints) + differences:
                        unknown = [v for v in expression.variables if not v.startswith(
                            ("argument_", "saved_r", "initial_", "response_"))]
                        if unknown:
                            raise RuntimeError(f"unmodeled symbolic inputs {unknown}")
                    solver.add([cp.Or(*differences)])
                    pairs += 1
                    if self.solve(solver, "observable difference"):
                        return {"name": name, "status": "counterexample",
                                "arguments": [solver.eval(x, 1)[0] for x in self.args],
                                "paths": [len(left), len(right)]}
            if pairs == 0:
                raise RuntimeError("no feasible paired paths")
            return {"name": name, "status": "unsat for modeled difference",
                    "reference_rva": hex(original), "rebuilt_address": hex(symbol.rebased_addr & ~1),
                    "paths": [len(left), len(right)], "feasible_pairs": pairs,
                    "block_steps": [ls, rs], "argument_widths": list(domains),
                    "return_observed": returns,
                    "fixed_arguments": {f"r{i}": hex(value) for i, value in self.fixed_arguments.items()},
                    "fixed_stack_arguments": {str(i): hex(value) for i, value in self.fixed_stack_arguments.items()}}
        except Exception as exc:
            return {"name": name, "status": "unresolved", "reason": repr(exc)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    proof = Proof(args.elf)
    specs = [
        ("__aeabi_llsl", 0x5F54, (32, 32, 32), "u64"),
        ("__aeabi_llsr", 0x5F76, (32, 32, 32), "u64"),
        ("tx_set_slope_cal_bypass", 0x2970, (32,), True),
        ("tx_set_df1_mantissa_override", 0x297C, (32, 32), True),
        ("tx_set_hf_coeff_bypass", 0x298C, (8,), True),
        ("rx_set_rssi_cal_override", 0x1BCC, (32, 32), True),
        ("rx_set_mxdc_i_override", 0x1BDC, (32, 32), True),
        ("rx_set_mxdc_q_override", 0x1BEC, (32, 32), True),
        ("rx_set_filter_bw_override", 0x1BFC, (8, 8), True),
        ("coex_is_active", 0x2964, (), True),
        ("loopback_is_active", 0x4958, (), True),
        ("loopback_corr0_done_isr", 0x44EC, (), False),
        ("loopback_corr1_done_isr", 0x4580, (), False),
        ("coex_enable_rx_gain_backoff", 0x28DC, (), False),
        ("coex_disable_rx_gain_backoff", 0x292C, (), False),
        ("modem_set_register_unlock", 0x5868, (8,), True),
        ("rf_test_set_correlator_magic", 0x27AC, (32,), True),
        ("tx_set_power_mode_override", 0x3658, (32,), True),
        ("nvic_enable_irq", 0x1B4, (32,), True),
        ("nvic_disable_irq", 0x1C4, (32,), True),
        ("nvic_clear_pending_irq", 0x1D4, (32,), True),
        ("clk_enable", 0x264, (), False),
        ("clk_enter_panic_mode", 0x2C0, (), False),
        ("clk_set_prescaler", 0x304, (8,), False),
        ("aux_timer_isr", 0x420, (), False),
        ("systick_isr", 0x434, (), False),
        ("systick_init", 0x444, (32, 1, 1), False),
        ("systick_now", 0x474, (), True),
        ("phy_set_synth_band", 0x5A7C, (1, 1, 32), True),
        ("rtk_set_irq_priority_level", 0x6210, (32,), True),
        ("rtk_configure_irq_priority", 0x1A4, (32,), True),
        ("nvv_validate_address", 0x4EA8, (32,), True),
        ("trace_ring_base", 0x3F4, (), True),
        ("trace_ring_size", 0x400, (), True),
        ("radio_revision_get", 0x4BB8, (), True),
        ("radio_revision_detect", 0x4B78, (), True),
        ("seq_timer_now", 0x4AE0, (), "u64"),
        ("seq_timer_tx_trigger", 0x4AEC, (), "u64"),
        ("seq_timer_rx_trigger", 0x4AF8, (), "u64"),
        ("warm_boot_detected", 0x4D58, (), True),
        ("warm_boot_set_state", 0x4D94, (32,), False),
    ]
    results = []
    for spec in specs:
        result = proof.check(*spec)
        results.append(result)
        print(json.dumps(result), flush=True)
    # Arbitrary descriptor bytes at stated fixed RAM addresses, including both
    # overlap directions. This is a finite address set, not all-pointer proof.
    for pointer in (0x4D280700,) + tuple(0x4D2805C1 + delta for delta in (-20, -16, -4, -1, 0, 1, 4, 16, 20)):
        result = proof.check("mac_send_crash_notification", 0x4EC, (32,), False,
                             fixed_arguments={0: pointer})
        results.append(result)
        print(json.dumps(result), flush=True)
    for command, output, length in (
        (0x4D280800, 0x4D280900, 0x4D280A00),
        (0x4D280800, 0, 0x4D280A00),
        (0x4D28AA84, 0x4D280900, 0x4D280A00),
        (0x4D28B664, 0x4D280900, 0x4D280A00),
        (0x4D28AAA4, 0x4D280900, 0x4D280A00),
        (0x4D280800, 0x4D28AAAC, 0x4D280A00),
        (0x4D280800, 0x4D280900, 0x4D28AAA8),
        (0x4D160000, 0x4D280900, 0x4D160200),
    ):
        result = proof.check("dpd_handle_command", 0x3684, (32, 32, 32), True,
                             fixed_arguments={0: command, 1: output, 2: length})
        results.append(result)
        print(json.dumps(result), flush=True)
    # Positive control: change a zero return to one in an in-memory ELF clone.
    mutant = Proof(args.elf)
    symbol = mutant.rebuilt.loader.find_symbol("tx_set_slope_cal_bypass")
    code = mutant.rebuilt.loader.memory.load(symbol.rebased_addr & ~1, symbol.size)
    instructions = mutant.rebuilt.arch.capstone_thumb.disasm(code, symbol.rebased_addr & ~1)
    instruction = next(i for i in instructions if i.mnemonic == "movs" and i.op_str == "r0, #0")
    before = mutant.rebuilt.loader.memory.load(instruction.address, 2)
    if before != b"\x00\x20":
        raise RuntimeError("unexpected positive-control instruction")
    mutant.rebuilt.loader.memory.store(instruction.address, b"\x01\x20")
    changed_return = mutant.check("tx_set_slope_cal_bypass", 0x2970, (32,), True)
    if changed_return["status"] != "counterexample":
        raise RuntimeError(f"verifier did not detect changed return: {changed_return}")

    class MissingPath(Proof):
        def run(self, project, address, domains):
            paths, steps = super().run(project, address, domains)
            return (paths[1:] if project is self.reference else paths), steps

    missing = MissingPath(args.elf)
    omitted_path = missing.check("rtk_set_irq_priority_level", 0x6210, (32,), True)
    if "do not cover" not in omitted_path.get("reason", ""):
        raise RuntimeError(f"verifier did not detect missing coverage: {omitted_path}")
    pointer = Proof(args.elf)
    symbolic_pointer = pointer.check("xor_checksum_words", 0x4E88, (32, 32), True)
    if "unbounded/non-ROM symbolic read" not in symbolic_pointer.get("reason", ""):
        raise RuntimeError(f"verifier did not reject symbolic stores: {symbolic_pointer}")
    class InvertedGuard(Proof):
        def write_before(self, state):
            address = self.concrete(state.inspect.attrs.mem_write_address, "write address")
            condition = state.inspect.attrs.mem_write_condition
            if state.project is self.rebuilt and peripheral(address):
                state.inspect.attrs.mem_write_condition = cp.Not(self.guard(condition))
            super().write_before(state)
            state.inspect.attrs.mem_write_condition = condition

    guarded = InvertedGuard(args.elf)
    inverted_guard = guarded.check("aux_timer_isr", 0x420, (), False)
    if inverted_guard["status"] != "counterexample":
        raise RuntimeError(f"verifier did not detect inverted event guard: {inverted_guard}")
    # A missing correlator-0 count store must produce a counterexample under
    # the same arbitrary accumulator/count/active-byte domain as the new checks.
    count_mutant = Proof(args.elf)
    symbol = count_mutant.rebuilt.loader.find_symbol("lb_accumulate")
    code = count_mutant.rebuilt.loader.memory.load(symbol.rebased_addr & ~1, symbol.size)
    stores = [i for i in count_mutant.rebuilt.arch.capstone_thumb.disasm(code, symbol.rebased_addr & ~1)
              if i.mnemonic == "strb"]
    if len(stores) != 1 or stores[0].size != 2:
        raise RuntimeError("unexpected loopback count-store encoding")
    count_mutant.rebuilt.loader.memory.store(stores[0].address, b"\x00\xbf")
    missing_count = count_mutant.check("loopback_corr0_done_isr", 0x44EC, (), False)
    if missing_count["status"] != "counterexample":
        raise RuntimeError(f"verifier did not detect missing loopback count store: {missing_count}")
    crash_mutant = Proof(args.elf)
    symbol = crash_mutant.rebuilt.loader.find_symbol("mac_send_crash_notification")
    code = crash_mutant.rebuilt.loader.memory.load(symbol.rebased_addr & ~1, symbol.size)
    first_load = next(i for i in crash_mutant.rebuilt.arch.capstone_thumb.disasm(code, symbol.rebased_addr & ~1)
                      if i.mnemonic == "ldr" and i.op_str == "r1, [r0]")
    if first_load.size != 2:
        raise RuntimeError("unexpected crash first-word load encoding")
    crash_mutant.rebuilt.loader.memory.store(first_load.address, b"\x00\x21")  # MOVS R1,#0
    changed_crash_word = crash_mutant.check("mac_send_crash_notification", 0x4EC, (32,), False,
                                           fixed_arguments={0: 0x4D280700})
    if changed_crash_word["status"] != "counterexample":
        raise RuntimeError(f"verifier did not detect changed crash word: {changed_crash_word}")
    smt_dir = args.output.parent / "symbolic-smt"
    smt_dir.mkdir(parents=True, exist_ok=True)
    queries = []
    for owner, model in (("routines", proof), ("changed-return", mutant),
                         ("missing-path", missing), ("symbolic-pointer", pointer),
                         ("inverted-guard", guarded), ("loopback-count", count_mutant), ("crash-word", crash_mutant)):
        for label, satisfiable, text in model.queries:
            path = smt_dir / f"{len(queries):04d}-{owner}.smt2"
            path.write_text(text)
            replay = z3.Solver()
            replay.set(timeout=10000)
            replay.from_string(text)
            expected = "sat" if satisfiable else "unsat"
            actual = str(replay.check())
            if actual != expected:
                raise RuntimeError(f"SMT-LIB replay mismatch {path}: {expected} != {actual}")
            queries.append({"label": label, "path": str(path),
                            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                            "result": expected, "z3_python_replay": actual})
    report = {"scope": "conditional scalar binary equivalence under declared model; not full firmware proof",
              "engine": "angr/PyVEX",
              "versions": {name: importlib.metadata.version(name)
                           for name in ("angr", "pyvex", "tibs", "z3-solver")},
              "verifier_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "source_elf_unchanged": proof.elf == args.elf.read_bytes(),
              "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(proof.elf).hexdigest(),
              "assumptions": ["PyVEX/SMT semantics are correct", "PyVEX ARM_COPY models all 16 initial NZCV patterns",
                              "initialized pointer cells retain image values", "MMIO responses are independent symbolic values per read",
                              "declared AAPCS argument widths", "interrupts do not intervene"],
              "routine_count": len({r["name"] for r in results}),
              "check_count": len(results),
              "smt_queries": queries,
              "self_tests": {"changed_return": changed_return,
                             "missing_path": omitted_path,
                             "symbolic_pointer_rejected": symbolic_pointer,
                             "inverted_event_guard": inverted_guard,
                             "missing_loopback_count_store": missing_count,
                             "changed_crash_word": changed_crash_word},
              "results": results}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    return any(x["status"] != "unsat for modeled difference" for x in results) or not report["source_elf_unchanged"]


if __name__ == "__main__":
    raise SystemExit(main())
