/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef GDB_ARCH_HH
#define GDB_ARCH_HH

#include "gdb_stub.hh"
#include <osv/sched.hh>

namespace gdb_stub {

#ifdef __x86_64__

// x86_64 register layout for GDB
struct x64_registers {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, eflags;
    uint32_t cs, ss, ds, es, fs, gs;
    // FPU/SSE registers would go here
};

class x64_arch_interface : public arch_interface {
public:
    virtual std::vector<register_info> get_register_info() override;
    virtual bool read_registers(sched::thread* thread, std::vector<uint8_t>& data) override;
    virtual bool write_registers(sched::thread* thread, const std::vector<uint8_t>& data) override;
    virtual bool read_register(sched::thread* thread, int reg_num, std::vector<uint8_t>& data) override;
    virtual bool write_register(sched::thread* thread, int reg_num, const std::vector<uint8_t>& data) override;
    
    virtual bool set_breakpoint(breakpoint& bp) override;
    virtual bool remove_breakpoint(const breakpoint& bp) override;
    virtual bool is_breakpoint_instruction(uintptr_t addr) override;
    
    virtual bool single_step(sched::thread* thread) override;
    virtual std::string get_target_xml() override;
    virtual size_t get_instruction_size(uintptr_t addr) override;

private:
    bool get_thread_registers(sched::thread* thread, x64_registers& regs);
    bool set_thread_registers(sched::thread* thread, const x64_registers& regs);
};

#endif // __x86_64__

#ifdef __aarch64__

// AArch64 register layout for GDB
struct aarch64_registers {
    uint64_t x[31];  // X0-X30
    uint64_t sp;     // Stack pointer
    uint64_t pc;     // Program counter
    uint32_t cpsr;   // Current program status register
    // FPU/NEON registers would go here
};

class aarch64_arch_interface : public arch_interface {
public:
    virtual std::vector<register_info> get_register_info() override;
    virtual bool read_registers(sched::thread* thread, std::vector<uint8_t>& data) override;
    virtual bool write_registers(sched::thread* thread, const std::vector<uint8_t>& data) override;
    virtual bool read_register(sched::thread* thread, int reg_num, std::vector<uint8_t>& data) override;
    virtual bool write_register(sched::thread* thread, int reg_num, const std::vector<uint8_t>& data) override;
    
    virtual bool set_breakpoint(breakpoint& bp) override;
    virtual bool remove_breakpoint(const breakpoint& bp) override;
    virtual bool is_breakpoint_instruction(uintptr_t addr) override;
    
    virtual bool single_step(sched::thread* thread) override;
    virtual std::string get_target_xml() override;
    virtual size_t get_instruction_size(uintptr_t addr) override;

private:
    bool get_thread_registers(sched::thread* thread, aarch64_registers& regs);
    bool set_thread_registers(sched::thread* thread, const aarch64_registers& regs);
};

#endif // __aarch64__

} // namespace gdb_stub

#endif // GDB_ARCH_HH