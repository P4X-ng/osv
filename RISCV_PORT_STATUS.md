# RISC-V Port Implementation Status

This document tracks the implementation status of the RISC-V port for OSv.

## Overview

The RISC-V port adds support for the RISC-V 64-bit (riscv64) architecture to OSv.
This implementation follows the same patterns established by the existing x64 and 
aarch64 ports.

## Completed Work

### Phase 1: Build System and Infrastructure ✅

1. **Makefile** - Added RISC-V architecture detection
   - Added `__riscv` detection to `detect_arch` function
   - Architecture is detected and set to `riscv64`

2. **conf/riscv64.mk** - RISC-V compiler configuration
   - Set TLS model to local-exec for consistency
   - Added RISCV64_PORT_STUB define

3. **conf/profiles/riscv64/** - Platform profiles
   - `base.mk` - Base driver configuration
   - `virtio-mmio.mk` - VirtIO MMIO driver configuration
   - `all.mk` - Includes all profiles

4. **scripts/build** - Build script updates
   - Added riscv64 to supported architectures in help text

### Phase 2: Core Architecture Implementation ✅

All essential architecture files have been created in `arch/riscv64/`:

1. **arch.hh** - Core architecture interface
   - IRQ control using RISC-V CSR instructions (sstatus register)
   - SIE bit manipulation for interrupt enable/disable
   - Thread pointer (tp) register for TLS checking
   - Cache line alignment (64 bytes)
   - Instruction size minimum (4 bytes - RISC-V base)

2. **processor.hh** - Low-level CPU operations
   - WFI (Wait For Interrupt) instruction
   - CSR access for sstatus (supervisor status register)
   - Cycle counter reading via rdcycle instruction
   - SATP (Supervisor Address Translation and Protection) register access
   - FPU state structure (32 64-bit registers + fcsr)

3. **arch-mmu.hh** - Memory Management Unit
   - Sv39 paging scheme support (39-bit virtual addressing)
   - Page table entry structure with RISC-V PTE bits:
     - V (Valid), R (Read), W (Write), X (Execute)
     - U (User), G (Global), A (Accessed), D (Dirty)
   - Physical Page Number (PPN) in bits 10-53
   - TLB flush via sfence.vma instruction

4. **arch-elf.hh/cc** - ELF support
   - RISC-V relocation types (R_RISCV_*)
   - ELF machine type 243 (EM_RISCV)
   - Entry point execution with proper calling convention
   - Stack alignment (16-byte)

5. **arch-switch.hh** - Context switching
   - Thread state save/restore framework
   - Stack pointer and return address handling
   - Thread pointer (tp) register management

6. **arch-thread-state.hh** - Thread state
   - Saved registers: sp, ra, s0-s11, tp
   - Structure for context switching

7. **Other supporting files**:
   - arch-cpu.hh/cc - CPU initialization stubs
   - arch-setup.hh/cc - Platform setup stubs
   - arch-tls.hh - TLS support (tp register)
   - arch-interrupt.hh - Interrupt handling framework
   - arch-trace.hh/cc - Tracing support stubs
   - arch-pci.hh - PCI support stub
   - arch-dtb.hh/cc - Device tree support stubs

### Phase 3: Platform-Specific Code Updates (Partial) ✅

Updated 6 key files to support RISC-V:

1. **include/osv/elf.hh**
   - Added RISC-V dynamic linker: `ld-linux-riscv64-lp64d.so.1`

2. **include/osv/debug.h**
   - Added early debug entry using `ra` (return address) register

3. **drivers/virtio-mmio.hh**
   - Extended for RISC-V with SPI interrupt support

4. **drivers/virtio-device.hh**
   - Added RISC-V SPI edge interrupt factory

5. **core/spinlock.cc**
   - Added RISC-V pause equivalent: `isb sy` instruction

6. **scripts/loader.py**
   - Added riscv64 architecture recognition

## Remaining Work

### High Priority - Required for Basic Boot

1. **Core System Files** (loader.cc, linux.cc)
   - Need RISC-V-specific implementations for:
     - System call handling
     - ELF program loading
     - Signal handling
     - Architecture-specific initialization

2. **Memory Management** (core/mmu.cc, core/elf.cc)
   - RISC-V page table setup
   - TLB management
   - Memory mapping

3. **Scheduler** (core/sched.cc)
   - RISC-V-specific context switch implementation
   - FPU state save/restore
   - Thread initialization

4. **Include Files** (include/osv/sched.hh)
   - Multiple architecture-specific sections need RISC-V support

5. **Assembly Files**
   - boot.S - Boot sequence and early initialization
   - entry.S - Exception/interrupt entry points
   - sched.S - Context switch assembly
   - elf-dl.S - Dynamic linker support

### Medium Priority - Required for Full Functionality

6. **Device Drivers**
   - Complete virtio-mmio implementation
   - UART/console driver
   - Timer/clock driver
   - Interrupt controller (PLIC)

7. **Libc Integration**
   - libc/pthread.cc - Threading support
   - libc/vdso/vdso.cc - Virtual DSO for RISC-V

8. **Remaining Driver Updates**
   - virtio-blk.cc, virtio-net.cc, virtio-fs.cc
   - Other drivers with architecture-specific code

### Low Priority - Optional/Testing

9. **Test Suite Updates**
   - Update tests with RISC-V support
   - Architecture-specific test cases

10. **Build System Enhancements**
    - QEMU RISC-V support in scripts/run.py
    - Cross-compilation toolchain download scripts
    - RISC-V-specific build profiles

11. **Musl C Library**
    - Create include/api/riscv64/ directory
    - Add RISC-V-specific headers (bits/*.h)

## Key Design Decisions

### 1. Paging Scheme
- **Choice**: Sv39 (39-bit virtual addressing)
- **Rationale**: Balanced between Sv48 (more complex) and Sv32 (32-bit only)
- **Details**: 3-level page table, 4KB pages, 512 entries per level

### 2. Interrupt Handling
- **Choice**: sstatus CSR for interrupt control
- **Rationale**: Standard RISC-V supervisor mode approach
- **Details**: SIE bit enables/disables supervisor interrupts

### 3. TLS (Thread Local Storage)
- **Choice**: tp register (x4)
- **Rationale**: RISC-V ABI standard for TLS
- **Details**: Points to thread control block (TCB)

### 4. Calling Convention
- **Choice**: RISC-V LP64D ABI
- **Rationale**: Standard 64-bit RISC-V ABI with double-precision FPU
- **Details**: Arguments in a0-a7, return address in ra

### 5. Platform Similarity
- **Choice**: Group RISC-V with aarch64 where applicable
- **Rationale**: Both use device tree, MMIO, similar interrupt models
- **Details**: SPI interrupts, virtio-mmio drivers

## Build Instructions

### Prerequisites
```bash
# Install RISC-V toolchain
# Example for Ubuntu:
sudo apt-get install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu

# Or download from RISC-V organization
```

### Building
```bash
# Set architecture
export ARCH=riscv64

# Build OSv kernel (when implementation is complete)
./scripts/build arch=riscv64 mode=debug
```

### Testing with QEMU
```bash
# Run with QEMU RISC-V (when scripts are updated)
./scripts/run.py -a riscv64
```

## References

1. **RISC-V Specifications**
   - Privileged Architecture: https://riscv.org/technical/specifications/
   - ISA Manual: https://github.com/riscv/riscv-isa-manual

2. **Existing OSv Ports**
   - x64: arch/x64/
   - aarch64: arch/aarch64/
   - PORTING document in repository root

3. **RISC-V Resources**
   - QEMU RISC-V: https://www.qemu.org/docs/master/system/target-riscv.html
   - OpenSBI firmware: https://github.com/riscv-software-src/opensbi
   - Linux RISC-V port: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/arch/riscv

## Contributing

This is an initial implementation with many stub functions. To contribute:

1. Look for TODO comments in arch/riscv64/ files
2. Review the "Remaining Work" section above
3. Follow patterns from existing x64 and aarch64 implementations
4. Test changes with QEMU RISC-V when available

## Status Summary

- **Phase 1** (Build System): ✅ Complete
- **Phase 2** (Core Architecture): ✅ Complete (stubs in place)
- **Phase 3** (Platform Updates): 🔄 In Progress (6 of ~32 files)
- **Phase 4** (Debug Tools): 🔄 In Progress (1 of 3 files)
- **Phase 5** (Musl Support): ⏸️ Not Started
- **Phase 6** (Testing): ⏸️ Not Started

**Overall Progress**: ~30% complete

The foundation is in place. The main work remaining is implementing the 
architecture-specific functionality in the stub files and updating the remaining
platform-specific code throughout the codebase.
