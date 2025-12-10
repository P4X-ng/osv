# GDB Stub Module for OSv

This module provides GDB remote debugging support for OSv, allowing you to debug OSv kernel and applications using GDB.

## Features

- **Multiple Transport Options**: Supports TCP and serial port communication
- **Standard GDB Protocol**: Implements GDB Remote Serial Protocol
- **Basic Commands**: Supports reading/writing registers and memory
- **Breakpoint Support**: Software breakpoints (hardware breakpoints planned)
- **Thread-safe**: Runs in a separate OSv thread

## Building

The GDB stub module is built as part of the OSv build process:

```bash
# Build OSv with GDB stub module
./scripts/build image=gdb-stub

# Or add it to another image
./scripts/build image=native-example,gdb-stub
```

## Usage

### TCP Transport (Recommended)

Start OSv with the GDB stub module and specify a TCP port:

```bash
# Build the image
./scripts/build image=gdb-stub,native-example

# Run OSv with GDB stub on port 1234 (default)
./scripts/run.py -e '/libgdb-stub.so --gdb-tcp 1234 &! /hello'

# Or use a custom port
./scripts/run.py -e '/libgdb-stub.so --gdb-tcp 9000 &! /hello'
```

Then connect from GDB on your host:

```bash
# Start GDB
gdb

# Connect to OSv
(gdb) target remote :1234

# Now you can use GDB commands
(gdb) info registers
(gdb) x/10i $rip
(gdb) break main
(gdb) continue
```

### Serial Transport

For serial port debugging (useful for systems without network):

```bash
# Run OSv with GDB stub on serial port
./scripts/run.py -e '/libgdb-stub.so --gdb-serial /dev/ttyS0'
```

Connect from GDB:

```bash
gdb
(gdb) target remote /dev/ttyS0
```

## Command Line Options

- `--gdb-tcp <port>`: Start GDB stub on specified TCP port (default: 1234)
- `--gdb-serial <device>`: Start GDB stub on specified serial device
- `--gdb-help`: Show help message

## Supported GDB Commands

The current implementation supports:

- **Register Operations**: `info registers`, reading/writing registers
- **Memory Operations**: `x` (examine memory), reading/writing memory
- **Breakpoints**: Software breakpoints (insert/remove)
- **Execution Control**: `continue`, `step` (basic support)
- **Query Commands**: Target information, thread queries

## Limitations

- Single-threaded debugging (multi-thread support planned)
- Basic breakpoint support (hardware breakpoints not yet implemented)
- Limited CPU state capture (enhancement in progress)
- Some advanced GDB features not yet supported

## Examples

### Example 1: Debug a Native Application

```bash
# Build with debug symbols
./scripts/build image=gdb-stub,native-example

# Run with GDB stub
./scripts/run.py -e '/libgdb-stub.so --gdb-tcp 1234 &! /hello'

# In another terminal
gdb /path/to/hello
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

### Example 2: Debug OSv Kernel

```bash
# Build kernel with debug info
./scripts/build image=gdb-stub

# Run with GDB stub
./scripts/run.py -e '/libgdb-stub.so --gdb-tcp 1234'

# In another terminal
gdb build/release.x64/loader.elf  # Or build/release.aarch64/loader.elf for ARM
(gdb) target remote :1234
(gdb) info threads
```

### Example 3: Examine Memory

```bash
# After connecting with GDB
(gdb) x/20i $rip      # Examine 20 instructions at RIP
(gdb) x/10gx $rsp     # Examine 10 8-byte values at RSP
(gdb) x/s 0x12345678  # Examine string at address
```

## Troubleshooting

### Connection Refused

If you get "Connection refused", ensure:
1. OSv has started and the GDB stub is running
2. The port is correct and not blocked by firewall
3. Network connectivity is available (for TCP mode)

### Cannot Read Registers

This may happen if:
1. The CPU state capture is not working correctly
2. OSv is in an unexpected state

Try:
- Checking OSv console output for errors
- Restarting OSv and GDB
- Using simpler commands first (e.g., query commands)

### Breakpoints Not Working

Current implementation has basic breakpoint support. For better results:
- Use software breakpoints (`break`) rather than hardware ones
- Ensure the address is valid and executable
- Check that memory is writable for breakpoint insertion

## Architecture Support

Currently tested on:
- **x86_64**: Full support
- **aarch64**: Planned (register layout differs)

## Related Links

- [GDB Remote Serial Protocol Documentation](https://sourceware.org/gdb/current/onlinedocs/gdb/Remote-Protocol.html)
- [OSv Debugging Wiki](https://github.com/cloudius-systems/osv/wiki/Debugging-OSv)
- [GDB User Manual](https://sourceware.org/gdb/documentation/)

## Contributing

To enhance the GDB stub:

1. **Add more protocol commands**: Implement additional GDB protocol commands
2. **Improve CPU state capture**: Better integration with OSv thread/CPU state
3. **Add hardware breakpoints**: Use CPU debug registers
4. **Multi-threading support**: Support debugging multiple threads
5. **Architecture support**: Add aarch64 register layouts

## License

This module is licensed under the BSD license, same as OSv.
