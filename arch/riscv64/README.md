# RISC-V Architecture Support

This directory contains the RISC-V 64-bit (riscv64) architecture-specific implementation for OSv.

## Architecture Overview

RISC-V is an open-source instruction set architecture (ISA) based on reduced instruction set computer (RISC) principles. This implementation targets the RV64GC variant:
- RV64I: 64-bit base integer instruction set
- M: Integer multiplication and division
- A: Atomic instructions
- F: Single-precision floating-point
- D: Double-precision floating-point
- C: Compressed instructions

## Key Features

### Memory Management
- **Paging Scheme**: Sv39 (39-bit virtual addressing)
- **Page Size**: 4KB
- **Page Table Levels**: 3
- **Physical Address**: Up to 56 bits

### Interrupt Handling
- **Interrupt Control**: sstatus CSR (Supervisor Status Register)
- **Interrupt Enable**: SIE bit (bit 1)
- **Vector Table**: stvec CSR (Supervisor Trap Vector)
- **Exception Handling**: scause CSR (Supervisor Cause Register)

### Thread Local Storage
- **TLS Register**: tp (x4, thread pointer)
- **TLS Model**: Local-exec
- **ABI**: LP64D (64-bit with double-precision FPU)

## File Structure

### Core Files
- **arch.hh** - Architecture-independent interface
  - IRQ enable/disable functions
  - Wait for interrupt
  - TLS availability check

- **processor.hh** - Low-level processor operations
  - CSR access macros and functions
  - WFI instruction
  - Cycle counter access

### Memory Management
- **arch-mmu.hh** - MMU operations
  - Page table entry structure
  - PTE bit definitions
  - TLB flush operations

### ELF Support
- **arch-elf.hh/cc** - ELF loading and relocation
  - RISC-V relocation types
  - Dynamic linking support
  - Entry point execution

### Threading and Scheduling
- **arch-switch.hh** - Context switching
  - Thread state save/restore
  - Stack management
  - First thread switch

- **arch-thread-state.hh** - Thread state structure
  - Saved register set (sp, ra, s0-s11, tp)

### Setup and Initialization
- **arch-setup.hh/cc** - Platform initialization
  - Console setup
  - Memory initialization
  - Early console

- **arch-cpu.hh/cc** - CPU-specific operations
  - CPU initialization
  - Driver initialization

### Device Support
- **arch-dtb.hh/cc** - Device Tree support
  - DTB parsing
  - Hardware discovery

- **arch-pci.hh** - PCI bus support
  - PCI initialization

### Debugging and Tracing
- **arch-trace.hh/cc** - Tracing infrastructure
- **arch-interrupt.hh** - Interrupt management

### Miscellaneous
- **arch-tls.hh** - TLS support definitions

## Implementation Status

This is an initial implementation with stub functions in place. Many functions have TODO comments indicating where full implementation is needed.

### Completed
- ✅ Basic architecture interface
- ✅ MMU page table structures
- ✅ ELF relocation types
- ✅ Thread state structure
- ✅ CSR access functions

### In Progress
- 🔄 Context switching implementation
- 🔄 Interrupt handling
- 🔄 Device tree parsing
- 🔄 Boot sequence

### Not Started
- ⏸️ Assembly routines (boot.S, entry.S, sched.S)
- ⏸️ Exception handlers
- ⏸️ FPU state save/restore
- ⏸️ Timer/clock driver
- ⏸️ Interrupt controller (PLIC) driver

## Building for RISC-V

```bash
# Cross-compile OSv for RISC-V
make ARCH=riscv64

# Or using the build script
./scripts/build arch=riscv64
```

## Running on QEMU

```bash
# Install QEMU with RISC-V support
sudo apt-get install qemu-system-riscv64

# Run OSv (when implementation is complete)
qemu-system-riscv64 -machine virt -m 2G \
    -kernel build/release.riscv64/loader.elf \
    -append "root=/dev/vda1" \
    -drive file=build/release.riscv64/usr.img,format=raw,if=none,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -netdev user,id=net0 \
    -device virtio-net-device,netdev=net0
```

## Development Guidelines

### Adding New Functionality

1. **Follow existing patterns**: Look at x64 and aarch64 implementations
2. **Use RISC-V standard conventions**: Follow the privileged spec
3. **Document assumptions**: Add comments explaining RISC-V-specific details
4. **Test incrementally**: Use QEMU for testing

### Register Usage

RISC-V calling convention (LP64D ABI):
- **a0-a7** (x10-x17): Function arguments and return values
- **ra** (x1): Return address
- **sp** (x2): Stack pointer
- **gp** (x3): Global pointer
- **tp** (x4): Thread pointer (TLS)
- **t0-t6**: Temporary registers (caller-saved)
- **s0-s11**: Saved registers (callee-saved)

### CSR Usage

Important Control and Status Registers:
- **sstatus**: Supervisor status (interrupt enable, privilege mode)
- **sie**: Supervisor interrupt enable
- **sip**: Supervisor interrupt pending
- **stvec**: Supervisor trap handler base address
- **scause**: Supervisor trap cause
- **stval**: Supervisor trap value
- **sepc**: Supervisor exception program counter
- **satp**: Supervisor address translation and protection

## References

### Specifications
- [RISC-V ISA Manual](https://github.com/riscv/riscv-isa-manual)
- [RISC-V Privileged Specification](https://riscv.org/technical/specifications/)
- [RISC-V ABI Specification](https://github.com/riscv-non-isa/riscv-elf-psabi-doc)

### Implementations
- [Linux RISC-V Port](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/arch/riscv)
- [FreeBSD RISC-V Port](https://github.com/freebsd/freebsd-src/tree/main/sys/riscv)
- [OpenSBI Firmware](https://github.com/riscv-software-src/opensbi)

### Tools
- [QEMU RISC-V](https://www.qemu.org/docs/master/system/target-riscv.html)
- [GNU RISC-V Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)

## Contributing

To contribute to the RISC-V port:

1. Review the TODO comments in the source files
2. Implement missing functionality following RISC-V specifications
3. Test with QEMU RISC-V
4. Submit patches following OSv contribution guidelines

## Support

For questions or issues related to the RISC-V port:
- Open an issue on the OSv GitHub repository
- Discuss on the OSv development mailing list
- Reference the RISCV_PORT_STATUS.md document in the repository root
