from osv.modules import api

# GDB stub module
# This module provides GDB remote debugging support for OSv

# The shared library provides the GDB stub functionality
default = api.run('/libgdb-stub.so')
