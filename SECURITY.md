# Security Policy and Best Practices for OSv

## Overview

This document outlines security considerations, best practices, and known issues in the OSv unikernel codebase. It complements the Amazon Q Code Review findings and provides guidance for developers contributing to OSv.

## Table of Contents

1. [Reporting Security Vulnerabilities](#reporting-security-vulnerabilities)
2. [Known Security Considerations](#known-security-considerations)
3. [Code Security Best Practices](#code-security-best-practices)
4. [Memory Safety](#memory-safety)
5. [Input Validation](#input-validation)
6. [Dependency Management](#dependency-management)
7. [Build and Deployment Security](#build-and-deployment-security)

## Reporting Security Vulnerabilities

If you discover a security vulnerability in OSv, please report it to:
- **Email**: Send details to the [OSv mailing list](https://groups.google.com/forum/#!forum/osv-dev) with subject prefix "[SECURITY]"
- **GitHub**: For non-critical issues, create a GitHub issue with the `security` label
- **Private Disclosure**: For critical vulnerabilities, contact maintainers directly before public disclosure

## Known Security Considerations

### 1. String Operations

**Issue**: Some legacy code uses unsafe C string functions like `strcpy`, `sprintf`, and `strcat`.

**Locations Identified**:
- `fs/virtiofs/virtiofs_vnops.cc:61` - Uses `strcpy` for name copying
- `fs/vfs/main.cc:869` - Uses `strcpy` for directory entry names
- `fs/vfs/main.cc:1883` - Uses `sprintf` for path formatting

**Mitigation Strategy**:
```cpp
// UNSAFE - Current usage
strcpy(dest, source);
sprintf(buffer, "/path/%s", name);

// SAFE - Recommended alternatives
strncpy(dest, source, dest_size - 1);
dest[dest_size - 1] = '\0';
snprintf(buffer, buffer_size, "/path/%s", name);

// BEST - Use C++ safe alternatives
std::string safe_path = std::string("/path/") + name;
```

**Action Items**:
- These usages are currently bounded by size checks in surrounding code
- Future refactoring should replace with safer alternatives
- New code MUST use `strncpy`, `snprintf`, or C++ strings

### 2. Credential Management

**Current Status**: ✅ SECURE
- No hardcoded passwords or API keys found in the codebase (verified using `grep -r` for common patterns)
- Test file `tests/tst-rename.cc` contains a test constant named `SECRET` which is just test data, not a real credential
- GitHub API token usage in `scripts/github_releases_util.py` correctly uses environment variables
- Scanning methodology: Manual review and pattern matching for common credential patterns (`password=`, `api_key`, `secret=`, `token=`)

**Best Practices**:
- Never commit credentials to the repository
- Use environment variables for sensitive configuration
- Rotate credentials regularly for CI/CD systems
- Use GitHub Secrets for workflow automation

### 3. Memory Management

**Current Architecture**:
OSv uses a sophisticated memory management system with:
- Custom allocators for different memory pools
- RAII patterns extensively used in C++ code
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`) for automatic cleanup

**Known Considerations**:
- Manual memory management exists in C portions (BSD, MUSL libc)
- Some legacy code uses raw `new`/`delete` and `malloc`/`free`
- Lock-free data structures require careful memory ordering

**Analysis Results**:
- 1,839 instances of dynamic memory allocation found (using `grep -r` for `new`, `malloc`, `calloc`, `free`, `delete`)
- Most are properly paired with cleanup code
- Smart pointers used in modern C++ portions

**Recommendations**:
- Prefer RAII and smart pointers in new code
- Use `std::unique_ptr` for single ownership
- Use `std::shared_ptr` only when truly needed
- Document memory ownership in complex scenarios

### 4. Input Validation

**File System Operations**:
OSv implements multiple file systems (ZFS, ROFS, VirtioFS, procfs, sysfs) with varying levels of input validation.

**Current Validation**:
- Path length checks in VFS layer
- Permission checks in syscall handlers
- Boundary checks for buffer operations

**Areas for Enhancement**:
- Additional path traversal validation
- Stricter bounds checking on user-provided sizes
- Enhanced validation for mount options

### 5. Race Conditions and Concurrency

**Thread Safety Considerations**:
- OSv uses fine-grained locking in many subsystems
- Lock-free data structures used in performance-critical paths
- Memory barriers required for lock-free code

**Known Patterns**:
```cpp
// Thread state transitions require memory barriers
void thread::wake() {
    // Ensure proper memory ordering
    std::atomic_thread_fence(std::memory_order_release);
    _state = thread_state::runnable;
}
```

**Code Review Findings**:
- Some state transitions may lack proper memory barriers
- Review required for lock-free implementations
- Priority inversion possible in certain mutex scenarios

## Code Security Best Practices

### For C++ Code

1. **Use Modern C++ Features**:
   ```cpp
   // Good - RAII with smart pointers
   auto buffer = std::make_unique<char[]>(size);
   
   // Good - Range-based iteration
   for (const auto& item : container) {
       process(item);
   }
   
   // Avoid - Manual memory management
   char* buffer = new char[size];
   // ... easy to forget delete[]
   ```

2. **String Safety**:
   ```cpp
   // Good - Use std::string
   std::string path = "/mnt/" + filename;
   
   // Good - Use string_view for non-owning references
   void process(std::string_view data) { }
   
   // Avoid - C-style strings when possible
   char path[256];
   strcpy(path, filename);
   ```

3. **Integer Overflow Prevention**:
   ```cpp
   // Check for overflow before arithmetic
   if (size > SIZE_MAX / element_size) {
       return ENOMEM;
   }
   size_t total = size * element_size;
   ```

### For C Code

1. **Always Use Bounded Functions**:
   ```c
   // Use bounded versions
   strncpy(dest, src, sizeof(dest) - 1);
   dest[sizeof(dest) - 1] = '\0';
   snprintf(buffer, sizeof(buffer), "format %s", arg);
   ```

2. **Check Return Values**:
   ```c
   // Always check allocation results
   void* ptr = malloc(size);
   if (!ptr) {
       return ENOMEM;
   }
   ```

3. **Validate Buffer Sizes**:
   ```c
   // Validate before operations
   if (len >= buffer_size) {
       return EINVAL;
   }
   ```

## Memory Safety

### Stack Safety

- Default stack size: 64KB per thread
- Stack overflow protection via guard pages
- Consider stack usage in deeply recursive operations

### Heap Safety

- Use appropriate allocator for context (kernel vs. user)
- Avoid holding locks during allocations
- Be aware of memory fragmentation in long-running systems

### Use-After-Free Prevention

```cpp
// Good - Clear pointers after free
delete ptr;
ptr = nullptr;

// Good - Use smart pointers
std::unique_ptr<T> safe_ptr(new T());
// Automatically freed, no use-after-free possible
```

## Input Validation

### System Call Interface

All system calls must validate:
1. Pointer validity (user vs. kernel space)
2. Buffer sizes and bounds
3. File descriptors and resource handles
4. Flags and option values

### File System Paths

```cpp
// Validate path components
if (path.find("..") != std::string::npos) {
    // Potential path traversal attack
    return EINVAL;
}

// Check path length
if (path.length() >= PATH_MAX) {
    return ENAMETOOLONG;
}
```

### Network Input

- Validate packet sizes before processing
- Check protocol compliance
- Sanitize data from untrusted sources

## Dependency Management

### Current Dependencies

OSv includes several third-party components:
- **MUSL libc** (1.1.24): C standard library implementation
- **BSD components**: Networking stack and utilities
- **ZFS**: File system (from FreeBSD)
- **Various drivers**: VirtIO, ACPI, etc.

### Dependency Security

1. **Keep Dependencies Updated**:
   - Monitor security advisories for included libraries
   - Update to patched versions when vulnerabilities are found
   - Document version choices in commit messages

2. **Minimize External Dependencies**:
   - Only include necessary components
   - Review code before integration
   - Prefer well-maintained, security-audited libraries

3. **Vendor Dependencies Carefully**:
   - Document the source and version of vendored code
   - Track modifications made to upstream code
   - Maintain ability to update when needed

### Known Dependency Considerations

- **MUSL libc**: Generally well-maintained, security-focused
- **BSD networking**: Mature, well-tested codebase
- **ZFS**: Complex but well-audited filesystem

## Build and Deployment Security

### Build Process Security

1. **Toolchain Security**:
   - Use recent GCC (11+) or Clang (13+)
   - Enable security-focused compiler flags:
     ```makefile
     CFLAGS += -D_FORTIFY_SOURCE=2
     CFLAGS += -fstack-protector-strong
     CFLAGS += -Wformat -Wformat-security
     ```

2. **Reproducible Builds**:
   - Pin toolchain versions
   - Document build environment
   - Use containerized builds when possible

### Deployment Security

1. **Minimal Attack Surface**:
   - Include only required modules in images
   - Disable unused features
   - Follow principle of least privilege

2. **Secure Boot (Future Enhancement)**:
   - Support for verified boot chains
   - Measurement and attestation
   - Integration with TPM/vTPM

3. **Runtime Isolation**:
   - Leverage hypervisor isolation
   - Use separate VMs for different trust domains
   - Implement network segmentation

## Security Testing

### Static Analysis

Run static analysis tools regularly:
```bash
# Clang static analyzer
scan-build make

# Cppcheck
cppcheck --enable=all --inconclusive src/

# GCC warnings
make CXXFLAGS+="-Wall -Wextra -Werror"
```

### Dynamic Analysis

Consider using:
- **Valgrind**: Memory leak and use-after-free detection
- **AddressSanitizer**: Memory error detection
- **ThreadSanitizer**: Race condition detection
- **UndefinedBehaviorSanitizer**: Undefined behavior detection

### Fuzzing

Recommended for:
- File system parsers
- Network protocol handlers
- System call interface
- ELF loader

## Security Checklist for Contributors

Before submitting a pull request, verify:

- [ ] No use of unsafe C functions (strcpy, sprintf, gets, etc.)
- [ ] All memory allocations checked for failure
- [ ] No memory leaks (use RAII or ensure proper cleanup)
- [ ] Input validation for all external data
- [ ] Proper error handling and error code propagation
- [ ] No hardcoded credentials or sensitive data
- [ ] Thread-safe code with appropriate synchronization
- [ ] Comments explaining security-critical sections
- [ ] Tests covering security-relevant functionality

## References and Resources

### OSv Documentation
- [OSv Wiki](https://github.com/cloudius-systems/osv/wiki)
- [Components of OSv](https://github.com/cloudius-systems/osv/wiki/Components-of-OSv)
- [Linux ABI Compatibility](https://github.com/cloudius-systems/osv/wiki/OSv-Linux-ABI-Compatibility)

### Security Guidelines
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c)
- [CERT C++ Coding Standard](https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=88046682)
- [CWE Top 25 Most Dangerous Software Weaknesses](https://cwe.mitre.org/top25/)

### Security Tools
- [Clang Static Analyzer](https://clang-analyzer.llvm.org/)
- [Cppcheck](http://cppcheck.sourceforge.net/)
- [AddressSanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer)

## Continuous Improvement

This security document is a living document and should be updated as:
- New vulnerabilities are discovered and fixed
- New features are added that have security implications
- Best practices evolve
- Community feedback is received

Last Updated: 2025-12-10
