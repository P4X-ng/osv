#!/bin/bash
#
# Example script demonstrating how to use the GDB stub module
# This script shows the typical workflow for debugging OSv with GDB
#

set -e

echo "GDB Stub Module - Usage Examples"
echo "================================="
echo ""

# Check if we're in the OSv directory
if [ ! -f "scripts/build" ]; then
    echo "Error: This script must be run from the OSv root directory"
    exit 1
fi

# Example 1: Build OSv with GDB stub module
echo "Example 1: Building OSv with GDB stub"
echo "--------------------------------------"
echo "Command: ./scripts/build image=gdb-stub,native-example"
echo ""
echo "This builds an OSv image with:"
echo "  - GDB stub module (libgdb-stub.so)"
echo "  - Native example application (/hello)"
echo ""

# Example 2: Run OSv with GDB stub on TCP
echo "Example 2: Running OSv with GDB stub on TCP"
echo "--------------------------------------------"
echo "Command: ./scripts/run.py -e '/libgdb-stub.so --gdb-tcp 1234 &! /hello'"
echo ""
echo "This starts OSv with:"
echo "  - GDB stub listening on TCP port 1234"
echo "  - Hello world application"
echo ""
echo "Then from another terminal, connect with GDB:"
echo "  $ gdb"
echo "  (gdb) target remote :1234"
echo "  (gdb) info registers"
echo "  (gdb) x/20i \$rip"
echo ""

# Example 3: Run OSv with GDB stub on serial
echo "Example 3: Running OSv with GDB stub on serial"
echo "-----------------------------------------------"
echo "Command: ./scripts/run.py -e '/libgdb-stub.so --gdb-serial /dev/ttyS0 &! /hello'"
echo ""
echo "This starts OSv with:"
echo "  - GDB stub on serial port /dev/ttyS0"
echo "  - Hello world application"
echo ""
echo "Then from another terminal, connect with GDB:"
echo "  $ gdb"
echo "  (gdb) target remote /dev/ttyS0"
echo ""

# Example 4: Debug with symbols
echo "Example 4: Debugging with symbol information"
echo "---------------------------------------------"
echo "For debugging with symbols, build your application with -g flag"
echo "and load it in GDB:"
echo ""
echo "  $ gdb /path/to/your/application"
echo "  (gdb) target remote :1234"
echo "  (gdb) break main"
echo "  (gdb) continue"
echo "  (gdb) step"
echo "  (gdb) print variable_name"
echo ""

# Example 5: Debugging OSv kernel
echo "Example 5: Debugging OSv kernel itself"
echo "---------------------------------------"
echo "To debug the OSv kernel:"
echo ""
echo "  $ gdb build/release.x64/loader.elf"
echo "  (gdb) target remote :1234"
echo "  (gdb) break sched::thread::wait"
echo "  (gdb) continue"
echo ""

# Example 6: Common GDB commands
echo "Example 6: Common GDB commands for OSv debugging"
echo "-------------------------------------------------"
echo "  info registers          - Show CPU registers"
echo "  x/10i \$rip             - Examine 10 instructions at current PC"
echo "  x/20gx \$rsp            - Examine 20 quad-words at stack pointer"
echo "  break *0x12345678       - Set breakpoint at address"
echo "  continue                - Continue execution"
echo "  step                    - Single step"
echo "  backtrace               - Show call stack"
echo "  info threads            - Show all threads (if supported)"
echo ""

# Example 7: Troubleshooting
echo "Example 7: Troubleshooting"
echo "--------------------------"
echo "If GDB cannot connect:"
echo "  1. Check OSv console for 'GDB stub: Listening on port XXXX'"
echo "  2. Verify port is not blocked by firewall"
echo "  3. Ensure OSv has networking enabled (for TCP mode)"
echo "  4. Try ping/telnet to verify network connectivity"
echo ""
echo "If breakpoints don't work:"
echo "  1. Ensure you're using software breakpoints"
echo "  2. Verify the address is valid and executable"
echo "  3. Check memory permissions"
echo ""

echo "For more information, see modules/gdb-stub/README.md"
