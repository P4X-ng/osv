# OSv GDB Stub Module

This module provides GDB remote debugging capabilities for OSv, allowing developers to debug OSv applications and the kernel itself using GDB.

## Features

- **GDB Remote Protocol Support**: Implements the GDB remote serial protocol for debugging
- **Multiple Transport Options**: Supports TCP, serial, and vsock transports
- **Architecture Support**: Works on both x86_64 and AArch64 platforms
- **Memory Access**: Read and write memory in the target system
- **Register Access**: Read and write CPU registers
- **Breakpoint Support**: Set and remove software breakpoints
- **Thread Support**: Multi-thread debugging capabilities

## Usage

### Building

The GDB stub is built as an optional module. To include it in your OSv image:

```bash
# Build OSv with the GDB stub module
make image=gdb-stub
```

### Configuration

By default, the GDB stub listens on TCP port 1234. You can modify the configuration in `gdb_stub.cc`:

```cpp
// Change the port number
auto transport = create_tcp_transport(1234);  // Change 1234 to your desired port
```

### Connecting with GDB

1. Start OSv with the GDB stub module loaded
2. Connect from GDB:

```bash
# Start GDB with your OSv binary
gdb path/to/osv/kernel.elf

# Connect to the GDB stub
(gdb) target remote localhost:1234

# Now you can debug normally
(gdb) break main
(gdb) continue
(gdb) info registers
(gdb) x/10i $pc
```

### Transport Options

#### TCP Transport (Default)
```cpp
auto transport = create_tcp_transport(1234);
```

#### Serial Transport
```cpp
auto transport = create_serial_transport("/dev/ttyS0");
```

#### VSock Transport (Future)
```cpp
auto transport = create_vsock_transport(1234);  // Not yet implemented
```

## GDB Commands Supported

- `g` - Read all registers
- `G` - Write all registers  
- `m` - Read memory
- `M` - Write memory
- `c` - Continue execution
- `s` - Single step
- `Z` - Set breakpoint
- `z` - Remove breakpoint
- `H` - Set thread for operations
- `T` - Check if thread is alive
- `?` - Get halt reason
- `q` - Query commands (Supported, ThreadInfo, etc.)

## Architecture-Specific Features

### x86_64
- Full register set support (RAX, RBX, RCX, etc.)
- Software breakpoints using INT3 instruction
- Single stepping using trap flag

### AArch64  
- General purpose registers X0-X30, SP, PC, CPSR
- Software breakpoints using BRK instruction
- Basic register access (single stepping not yet implemented)

## Limitations

- Hardware breakpoints not yet implemented
- Watchpoints not yet implemented
- Some advanced GDB features may not be supported
- Thread context switching integration needs improvement
- Memory protection checks could be enhanced

## Development

### Adding New Transport Types

1. Inherit from the `transport` interface
2. Implement all virtual methods
3. Add factory function in `gdb_transport.cc`

### Adding Architecture Support

1. Create architecture-specific class inheriting from `arch_interface`
2. Implement register layout and breakpoint handling
3. Add to the factory function in `gdb_arch.cc`

### Extending Protocol Support

Add new command handlers in `gdb_stub.cc` following the existing pattern.

## Troubleshooting

### Connection Issues
- Ensure the port is not blocked by firewall
- Check that OSv is running and the module is loaded
- Verify GDB is connecting to the correct address/port

### Debugging Issues
- Enable verbose debugging: set `verbose = true` in debug.cc
- Check OSv console output for GDB stub messages
- Use GDB's `set debug remote 1` for protocol debugging

## References

- [GDB Remote Serial Protocol](https://sourceware.org/gdb/current/onlinedocs/gdb/Remote-Protocol.html)
- [Implementing GDB Remote Debug Protocol](https://tatsuo.medium.com/implement-gdb-remote-debug-protocol-stub-from-scratch-6-1ac945e57399)