from osv.modules import api

# GDB stub module - provides remote debugging capabilities
# Supports TCP, serial, and vsock transports

# Run the GDB stub daemon
daemon = api.run('/libgdb-stub.so &!')
default = daemon