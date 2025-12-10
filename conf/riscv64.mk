# RISC-V 64-bit architecture configuration
# 
# RISC-V specific compiler flags and configuration
# Use the medany code model for kernel code to allow for flexible memory layout
# Force local-exec TLS model for consistent thread-local storage behavior
# Enable compressed instructions (C extension) if available
conf_compiler_cflags = -mcmodel=medany -ftls-model=local-exec -DRISCV64_PORT_STUB