/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb_arch.hh"
#include "gdb_stub.hh"
#include <osv/debug.hh>
#include <osv/sched.hh>
#include <cstring>
#include <string>

namespace gdb_stub {

#ifdef __x86_64__

// x86_64 breakpoint instruction (INT3)
const uint8_t X64_BREAKPOINT_INSTRUCTION = 0xCC;

std::vector<register_info> x64_arch_interface::get_register_info()
{
    std::vector<register_info> regs;
    
    // General purpose registers
    regs.push_back({"rax", 8, offsetof(x64_registers, rax), true});
    regs.push_back({"rbx", 8, offsetof(x64_registers, rbx), true});
    regs.push_back({"rcx", 8, offsetof(x64_registers, rcx), true});
    regs.push_back({"rdx", 8, offsetof(x64_registers, rdx), true});
    regs.push_back({"rsi", 8, offsetof(x64_registers, rsi), true});
    regs.push_back({"rdi", 8, offsetof(x64_registers, rdi), true});
    regs.push_back({"rbp", 8, offsetof(x64_registers, rbp), true});
    regs.push_back({"rsp", 8, offsetof(x64_registers, rsp), true});
    regs.push_back({"r8", 8, offsetof(x64_registers, r8), true});
    regs.push_back({"r9", 8, offsetof(x64_registers, r9), true});
    regs.push_back({"r10", 8, offsetof(x64_registers, r10), true});
    regs.push_back({"r11", 8, offsetof(x64_registers, r11), true});
    regs.push_back({"r12", 8, offsetof(x64_registers, r12), true});
    regs.push_back({"r13", 8, offsetof(x64_registers, r13), true});
    regs.push_back({"r14", 8, offsetof(x64_registers, r14), true});
    regs.push_back({"r15", 8, offsetof(x64_registers, r15), true});
    regs.push_back({"rip", 8, offsetof(x64_registers, rip), true});
    regs.push_back({"eflags", 4, offsetof(x64_registers, eflags), true});
    regs.push_back({"cs", 4, offsetof(x64_registers, cs), true});
    regs.push_back({"ss", 4, offsetof(x64_registers, ss), true});
    regs.push_back({"ds", 4, offsetof(x64_registers, ds), true});
    regs.push_back({"es", 4, offsetof(x64_registers, es), true});
    regs.push_back({"fs", 4, offsetof(x64_registers, fs), true});
    regs.push_back({"gs", 4, offsetof(x64_registers, gs), true});
    
    return regs;
}

bool x64_arch_interface::read_registers(sched::thread* thread, std::vector<uint8_t>& data)
{
    x64_registers regs;
    if (!get_thread_registers(thread, regs)) {
        return false;
    }
    
    data.resize(sizeof(x64_registers));
    memcpy(data.data(), &regs, sizeof(x64_registers));
    return true;
}

bool x64_arch_interface::write_registers(sched::thread* thread, const std::vector<uint8_t>& data)
{
    if (data.size() != sizeof(x64_registers)) {
        return false;
    }
    
    x64_registers regs;
    memcpy(&regs, data.data(), sizeof(x64_registers));
    return set_thread_registers(thread, regs);
}

bool x64_arch_interface::read_register(sched::thread* thread, int reg_num, std::vector<uint8_t>& data)
{
    x64_registers regs;
    if (!get_thread_registers(thread, regs)) {
        return false;
    }
    
    auto reg_info = get_register_info();
    if (reg_num >= static_cast<int>(reg_info.size())) {
        return false;
    }
    
    const auto& reg = reg_info[reg_num];
    data.resize(reg.size);
    memcpy(data.data(), reinterpret_cast<uint8_t*>(&regs) + reg.offset, reg.size);
    return true;
}

bool x64_arch_interface::write_register(sched::thread* thread, int reg_num, const std::vector<uint8_t>& data)
{
    auto reg_info = get_register_info();
    if (reg_num >= static_cast<int>(reg_info.size())) {
        return false;
    }
    
    const auto& reg = reg_info[reg_num];
    if (data.size() != reg.size) {
        return false;
    }
    
    x64_registers regs;
    if (!get_thread_registers(thread, regs)) {
        return false;
    }
    
    memcpy(reinterpret_cast<uint8_t*>(&regs) + reg.offset, data.data(), reg.size);
    return set_thread_registers(thread, regs);
}

bool x64_arch_interface::set_breakpoint(breakpoint& bp)
{
    if (bp.type != breakpoint_type::SOFTWARE) {
        // Hardware breakpoints not implemented yet
        return false;
    }
    
    // Read original instruction
    uint8_t* addr = reinterpret_cast<uint8_t*>(bp.address);
    bp.original_instruction = *addr;
    
    // Write breakpoint instruction
    *addr = X64_BREAKPOINT_INSTRUCTION;
    bp.enabled = true;
    
    return true;
}

bool x64_arch_interface::remove_breakpoint(const breakpoint& bp)
{
    if (bp.type != breakpoint_type::SOFTWARE || !bp.enabled) {
        return false;
    }
    
    // Restore original instruction
    uint8_t* addr = reinterpret_cast<uint8_t*>(bp.address);
    *addr = bp.original_instruction;
    
    return true;
}

bool x64_arch_interface::is_breakpoint_instruction(uintptr_t addr)
{
    uint8_t* instruction = reinterpret_cast<uint8_t*>(addr);
    return *instruction == X64_BREAKPOINT_INSTRUCTION;
}

bool x64_arch_interface::single_step(sched::thread* thread)
{
    // Set trap flag in EFLAGS to enable single stepping
    x64_registers regs;
    if (!get_thread_registers(thread, regs)) {
        return false;
    }
    
    regs.eflags |= 0x100; // Set TF (Trap Flag)
    return set_thread_registers(thread, regs);
}

std::string x64_arch_interface::get_target_xml()
{
    return R"(<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target version="1.0">
  <architecture>i386:x86-64</architecture>
  <feature name="org.gnu.gdb.i386.core">
    <reg name="rax" bitsize="64" type="int64"/>
    <reg name="rbx" bitsize="64" type="int64"/>
    <reg name="rcx" bitsize="64" type="int64"/>
    <reg name="rdx" bitsize="64" type="int64"/>
    <reg name="rsi" bitsize="64" type="int64"/>
    <reg name="rdi" bitsize="64" type="int64"/>
    <reg name="rbp" bitsize="64" type="data_ptr"/>
    <reg name="rsp" bitsize="64" type="data_ptr"/>
    <reg name="r8" bitsize="64" type="int64"/>
    <reg name="r9" bitsize="64" type="int64"/>
    <reg name="r10" bitsize="64" type="int64"/>
    <reg name="r11" bitsize="64" type="int64"/>
    <reg name="r12" bitsize="64" type="int64"/>
    <reg name="r13" bitsize="64" type="int64"/>
    <reg name="r14" bitsize="64" type="int64"/>
    <reg name="r15" bitsize="64" type="int64"/>
    <reg name="rip" bitsize="64" type="code_ptr"/>
    <reg name="eflags" bitsize="32" type="i386_eflags"/>
    <reg name="cs" bitsize="32" type="int32"/>
    <reg name="ss" bitsize="32" type="int32"/>
    <reg name="ds" bitsize="32" type="int32"/>
    <reg name="es" bitsize="32" type="int32"/>
    <reg name="fs" bitsize="32" type="int32"/>
    <reg name="gs" bitsize="32" type="int32"/>
  </feature>
</target>)";
}

size_t x64_arch_interface::get_instruction_size(uintptr_t addr)
{
    // Simple heuristic - x86_64 instructions are variable length
    // For now, assume minimum size of 1 byte
    return 1;
}

bool x64_arch_interface::get_thread_registers(sched::thread* thread, x64_registers& regs)
{
    // This would need to access the thread's saved context
    // For now, return dummy values
    memset(&regs, 0, sizeof(regs));
    
    // In a real implementation, we would extract registers from
    // the thread's saved context (e.g., from exception frame)
    debug("gdb-stub: get_thread_registers not fully implemented\n");
    return true;
}

bool x64_arch_interface::set_thread_registers(sched::thread* thread, const x64_registers& regs)
{
    // This would need to modify the thread's saved context
    // For now, just return success
    debug("gdb-stub: set_thread_registers not fully implemented\n");
    return true;
}

#endif // __x86_64__

#ifdef __aarch64__

// AArch64 breakpoint instruction (BRK #0)
const uint32_t AARCH64_BREAKPOINT_INSTRUCTION = 0xD4200000;

std::vector<register_info> aarch64_arch_interface::get_register_info()
{
    std::vector<register_info> regs;
    
    // General purpose registers X0-X30
    for (int i = 0; i < 31; i++) {
        regs.push_back({"x" + std::to_string(i), 8, offsetof(aarch64_registers, x) + i * 8, true});
    }
    
    regs.push_back({"sp", 8, offsetof(aarch64_registers, sp), true});
    regs.push_back({"pc", 8, offsetof(aarch64_registers, pc), true});
    regs.push_back({"cpsr", 4, offsetof(aarch64_registers, cpsr), true});
    
    return regs;
}

bool aarch64_arch_interface::read_registers(sched::thread* thread, std::vector<uint8_t>& data)
{
    aarch64_registers regs;
    if (!get_thread_registers(thread, regs)) {
        return false;
    }
    
    data.resize(sizeof(aarch64_registers));
    memcpy(data.data(), &regs, sizeof(aarch64_registers));
    return true;
}

bool aarch64_arch_interface::write_registers(sched::thread* thread, const std::vector<uint8_t>& data)
{
    if (data.size() != sizeof(aarch64_registers)) {
        return false;
    }
    
    aarch64_registers regs;
    memcpy(&regs, data.data(), sizeof(aarch64_registers));
    return set_thread_registers(thread, regs);
}

bool aarch64_arch_interface::read_register(sched::thread* thread, int reg_num, std::vector<uint8_t>& data)
{
    aarch64_registers regs;
    if (!get_thread_registers(thread, regs)) {
        return false;
    }
    
    auto reg_info = get_register_info();
    if (reg_num >= static_cast<int>(reg_info.size())) {
        return false;
    }
    
    const auto& reg = reg_info[reg_num];
    data.resize(reg.size);
    memcpy(data.data(), reinterpret_cast<uint8_t*>(&regs) + reg.offset, reg.size);
    return true;
}

bool aarch64_arch_interface::write_register(sched::thread* thread, int reg_num, const std::vector<uint8_t>& data)
{
    auto reg_info = get_register_info();
    if (reg_num >= static_cast<int>(reg_info.size())) {
        return false;
    }
    
    const auto& reg = reg_info[reg_num];
    if (data.size() != reg.size) {
        return false;
    }
    
    aarch64_registers regs;
    if (!get_thread_registers(thread, regs)) {
        return false;
    }
    
    memcpy(reinterpret_cast<uint8_t*>(&regs) + reg.offset, data.data(), reg.size);
    return set_thread_registers(thread, regs);
}

bool aarch64_arch_interface::set_breakpoint(breakpoint& bp)
{
    if (bp.type != breakpoint_type::SOFTWARE) {
        // Hardware breakpoints not implemented yet
        return false;
    }
    
    // Read original instruction (4 bytes for AArch64)
    uint32_t* addr = reinterpret_cast<uint32_t*>(bp.address);
    bp.original_instruction = *addr & 0xFF; // Store only first byte for compatibility
    
    // Write breakpoint instruction
    *addr = AARCH64_BREAKPOINT_INSTRUCTION;
    bp.enabled = true;
    
    return true;
}

bool aarch64_arch_interface::remove_breakpoint(const breakpoint& bp)
{
    if (bp.type != breakpoint_type::SOFTWARE || !bp.enabled) {
        return false;
    }
    
    // This is simplified - in reality we'd need to store the full 32-bit instruction
    uint32_t* addr = reinterpret_cast<uint32_t*>(bp.address);
    // For now, just clear the breakpoint - proper implementation would restore original
    *addr = 0; // NOP equivalent
    
    return true;
}

bool aarch64_arch_interface::is_breakpoint_instruction(uintptr_t addr)
{
    uint32_t* instruction = reinterpret_cast<uint32_t*>(addr);
    return *instruction == AARCH64_BREAKPOINT_INSTRUCTION;
}

bool aarch64_arch_interface::single_step(sched::thread* thread)
{
    // AArch64 single stepping would use debug registers or software emulation
    debug("gdb-stub: AArch64 single step not implemented\n");
    return false;
}

std::string aarch64_arch_interface::get_target_xml()
{
    return R"(<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target version="1.0">
  <architecture>aarch64</architecture>
  <feature name="org.gnu.gdb.aarch64.core">
    <reg name="x0" bitsize="64" type="int64"/>
    <reg name="x1" bitsize="64" type="int64"/>
    <reg name="x2" bitsize="64" type="int64"/>
    <reg name="x3" bitsize="64" type="int64"/>
    <reg name="x4" bitsize="64" type="int64"/>
    <reg name="x5" bitsize="64" type="int64"/>
    <reg name="x6" bitsize="64" type="int64"/>
    <reg name="x7" bitsize="64" type="int64"/>
    <reg name="x8" bitsize="64" type="int64"/>
    <reg name="x9" bitsize="64" type="int64"/>
    <reg name="x10" bitsize="64" type="int64"/>
    <reg name="x11" bitsize="64" type="int64"/>
    <reg name="x12" bitsize="64" type="int64"/>
    <reg name="x13" bitsize="64" type="int64"/>
    <reg name="x14" bitsize="64" type="int64"/>
    <reg name="x15" bitsize="64" type="int64"/>
    <reg name="x16" bitsize="64" type="int64"/>
    <reg name="x17" bitsize="64" type="int64"/>
    <reg name="x18" bitsize="64" type="int64"/>
    <reg name="x19" bitsize="64" type="int64"/>
    <reg name="x20" bitsize="64" type="int64"/>
    <reg name="x21" bitsize="64" type="int64"/>
    <reg name="x22" bitsize="64" type="int64"/>
    <reg name="x23" bitsize="64" type="int64"/>
    <reg name="x24" bitsize="64" type="int64"/>
    <reg name="x25" bitsize="64" type="int64"/>
    <reg name="x26" bitsize="64" type="int64"/>
    <reg name="x27" bitsize="64" type="int64"/>
    <reg name="x28" bitsize="64" type="int64"/>
    <reg name="x29" bitsize="64" type="int64"/>
    <reg name="x30" bitsize="64" type="int64"/>
    <reg name="sp" bitsize="64" type="data_ptr"/>
    <reg name="pc" bitsize="64" type="code_ptr"/>
    <reg name="cpsr" bitsize="32" type="int32"/>
  </feature>
</target>)";
}

size_t aarch64_arch_interface::get_instruction_size(uintptr_t addr)
{
    // AArch64 instructions are always 4 bytes
    return 4;
}

bool aarch64_arch_interface::get_thread_registers(sched::thread* thread, aarch64_registers& regs)
{
    // This would need to access the thread's saved context
    memset(&regs, 0, sizeof(regs));
    debug("gdb-stub: AArch64 get_thread_registers not fully implemented\n");
    return true;
}

bool aarch64_arch_interface::set_thread_registers(sched::thread* thread, const aarch64_registers& regs)
{
    debug("gdb-stub: AArch64 set_thread_registers not fully implemented\n");
    return true;
}

#endif // __aarch64__

// Factory function
std::unique_ptr<arch_interface> create_arch_interface()
{
#ifdef __x86_64__
    return std::make_unique<x64_arch_interface>();
#elif defined(__aarch64__)
    return std::make_unique<aarch64_arch_interface>();
#else
    debug("gdb-stub: Unsupported architecture\n");
    return nullptr;
#endif
}

} // namespace gdb_stub