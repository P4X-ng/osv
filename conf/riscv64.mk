# RISC-V 64-bit specific compiler flags
# Force all __thread variables to local exec TLS model for consistency
conf_compiler_cflags = -ftls-model=local-exec -DRISCV64_PORT_STUB
