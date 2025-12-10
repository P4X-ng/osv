# Code Cleanliness Guidelines for OSv

## Overview

This document provides guidelines for maintaining code quality and organization in the OSv project. It addresses findings from periodic automated code reviews and establishes policies for managing code complexity.

## Table of Contents

1. [File Size Guidelines](#file-size-guidelines)
2. [External Code Policy](#external-code-policy)
3. [When to Split Files](#when-to-split-files)
4. [When NOT to Split Files](#when-not-to-split-files)
5. [Code Organization Best Practices](#code-organization-best-practices)
6. [Refactoring Guidelines](#refactoring-guidelines)

## File Size Guidelines

### General Recommendations

- **Target**: Keep files under 500 lines when practical
- **Warning Level**: Files over 500 lines should be reviewed for potential splitting
- **Critical Level**: Files over 1000 lines are strong candidates for refactoring

### Context Matters

File size limits are guidelines, not hard rules. Consider:
- **Cohesion**: Does the file contain closely related functionality?
- **Complexity**: Is the code inherently complex (e.g., protocol implementations)?
- **Maintainability**: Can developers easily understand and modify the code?
- **External Code**: Third-party code should generally not be modified

## External Code Policy

### DO NOT MODIFY

The following categories of code are external/third-party and should **NOT** be split or significantly refactored:

#### 1. ZFS Implementation
- `bsd/sys/cddl/contrib/opensolaris/uts/common/fs/zfs/*`
- `bsd/cddl/contrib/opensolaris/lib/libzfs/*`
- `bsd/cddl/contrib/opensolaris/cmd/zfs/*`
- `bsd/cddl/contrib/opensolaris/cmd/zpool/*`
- `bsd/cddl/contrib/opensolaris/cmd/ztest/*`
- `bsd/cddl/contrib/opensolaris/cmd/zdb/*`

**Rationale**: ZFS is a complex, mature filesystem from OpenSolaris. Modifications can break compatibility and make upstream updates impossible.

#### 2. BSD Network Stack
- `bsd/sys/netinet/*` (TCP/IP implementation)
- `bsd/sys/netinet6/*` (IPv6 implementation)
- `bsd/sys/net/*` (Network interface code)
- `bsd/sys/kern/uipc_*.cc` (IPC and socket code)

**Rationale**: FreeBSD network stack is battle-tested. Splitting would diverge from upstream and complicate security updates.

#### 3. Cryptography and Compression
- `bsd/sys/crypto/*` (Rijndael, SHA2, etc.)
- `bsd/sys/cddl/contrib/opensolaris/uts/common/zmod/*` (zlib)
- `fastlz/fastlz.cc`

**Rationale**: Cryptographic code should not be modified without deep expertise. Maintaining compatibility with reference implementations is critical.

#### 4. Device Drivers from External Sources
- `bsd/sys/dev/xen/*` (Xen paravirtualization drivers)
- `bsd/sys/xen/*` (Xen interfaces)
- `bsd/sys/contrib/ena_com/*` (AWS ENA driver)
- `bsd/sys/dev/ena/*` (ENA driver integration)

**Rationale**: Drivers often track upstream implementations. Modifications make it harder to merge fixes and updates.

#### 5. External Libraries and Headers
- `external/aarch64/libfdt/*` (Flattened Device Tree library)
- `bsd/sys/sys/queue.h`, `tree.h`, `mbuf.h` (BSD data structures)
- Musl libc components (`musl_*` directories)

**Rationale**: Standard libraries and headers should remain compatible with their sources.

#### 6. Test Utilities from External Projects
- `modules/nfs-tests/fsx-linux.c` (File system exerciser)
- `tests/misc-fsx.c` (FSX test suite)

**Rationale**: Testing tools should match their reference implementations for compatibility.

### Minor Modifications Allowed

For external code, only these changes are acceptable:
- Bug fixes that will be submitted upstream
- Necessary adaptations for OSv (clearly marked with comments)
- Formatting changes required by OSv coding style (minimize these)

## When to Split Files

Consider splitting a file when:

### 1. Multiple Distinct Responsibilities

```cpp
// BAD: One file handling device, network, and filesystem
// device_manager.cc (1500 lines)
//   - Device initialization (500 lines)
//   - Network configuration (500 lines)
//   - Filesystem mounting (500 lines)

// GOOD: Split into focused files
// device_init.cc (500 lines)
// network_config.cc (500 lines)
// fs_mount.cc (500 lines)
```

### 2. Natural Module Boundaries

```cpp
// BAD: All memory management in one file
// memory.cc (2000 lines)
//   - Physical memory management
//   - Virtual memory management
//   - Memory pools
//   - Page cache

// GOOD: Split by functional area
// memory_physical.cc
// memory_virtual.cc
// memory_pool.cc
// page_cache.cc
```

### 3. Testing and Maintenance Burden

Large test files can often be split by test category:

```cpp
// BAD: One test file for all functionality
// tst-filesystem.cc (2000 lines)

// GOOD: Split by test area
// tst-filesystem-basic.cc
// tst-filesystem-permissions.cc
// tst-filesystem-performance.cc
// tst-filesystem-errors.cc
```

### 4. Python Scripts with Multiple Functions

Python scripts can often benefit from modularization:

```python
# BAD: Everything in one script
# loader.py (1758 lines)

# GOOD: Split into modules
# loader_core.py       # Core loading logic
# loader_manifest.py   # Manifest parsing
# loader_build.py      # Build system integration
# loader_image.py      # Image generation
```

## When NOT to Split Files

### 1. Cohesive Implementations

Do not split files that implement a single, cohesive concept:

```cpp
// KEEP TOGETHER: Protocol state machine
// tcp_input.cc (3289 lines)
// - Complex but cohesive TCP input processing
// - Splitting would fragment state machine logic
// - Understanding requires seeing the full picture
```

### 2. Core Kernel Components

Core kernel files often need to be large due to complexity:

- `core/sched.cc` (2099 lines) - Scheduler implementation
- `core/mempool.cc` (2232 lines) - Memory pool management
- `core/mmu.cc` (2134 lines) - Memory management unit
- `core/elf.cc` (2071 lines) - ELF loader

**Why**: These are inherently complex subsystems. Splitting them would:
- Break logical cohesion
- Increase inter-file dependencies
- Make understanding the subsystem harder
- Complicate debugging

### 3. Auto-Generated or Data Files

Some files are large by nature:

- `bsd/sys/cddl/contrib/opensolaris/uts/common/sys/u8_textprep_data.h` (35,376 lines)
  - Unicode normalization data tables
  - Generated from specifications
  - Not meant to be read by humans

### 4. Headers with API Definitions

Large header files defining APIs should often stay together:

- `include/osv/sched.hh` (1566 lines) - Scheduler API
- `bsd/sys/sys/mbuf.h` (1113 lines) - Network buffer API
- `include/osv/elf.hh` (792 lines) - ELF structures

**Why**: Splitting headers:
- Increases include complexity
- Breaks logical grouping of related APIs
- May cause circular dependency issues

## Code Organization Best Practices

### 1. Clear Directory Structure

Organize code by functional area:

```
osv/
├── core/           # Core kernel functionality
│   ├── sched.cc    # Scheduler
│   ├── mempool.cc  # Memory management
│   └── mmu.cc      # MMU
├── drivers/        # Device drivers
│   ├── virtio/     # Virtio devices
│   ├── pci/        # PCI subsystem
│   └── ahci/       # AHCI/SATA
├── fs/             # Filesystems
│   ├── vfs/        # VFS layer
│   ├── ramfs/      # RAM filesystem
│   └── virtiofs/   # VirtioFS
└── libc/           # C library
    ├── pthread.cc  # POSIX threads
    └── signal.cc   # Signal handling
```

### 2. Header Organization

Use forward declarations to minimize header dependencies:

```cpp
// GOOD: Forward declarations
// my_module.hh
#pragma once

namespace my_module {
    class impl;  // Forward declaration
    
    class interface {
        void method();
    private:
        std::unique_ptr<impl> _impl;
    };
}

// my_module.cc includes full implementation
```

### 3. Implementation Files

Keep implementation details private:

```cpp
// GOOD: Anonymous namespace for internal functions
// my_module.cc
namespace {
    // Internal helper functions not in header
    void internal_helper() { ... }
}

namespace my_module {
    void interface::method() {
        internal_helper();
    }
}
```

### 4. Module Boundaries

Respect module boundaries:

```
modules/
├── my_app/
│   ├── Makefile
│   ├── module.py
│   ├── src/
│   │   ├── main.cc
│   │   ├── handler.cc
│   │   └── utils.cc
│   └── include/
│       └── my_app/
│           ├── handler.hh
│           └── utils.hh
```

## Refactoring Guidelines

### 1. Incremental Refactoring

When refactoring large files:

1. **Identify** clear subsections that can be extracted
2. **Create** new files with appropriate names
3. **Move** code while maintaining API compatibility
4. **Test** after each extraction
5. **Document** the changes

### 2. Extract Class/Function

For complex functions, extract helper classes or functions:

```cpp
// BEFORE: One large function
void process_data(const data& d) {
    // 200 lines of parsing
    // 200 lines of validation
    // 200 lines of processing
}

// AFTER: Extracted helpers
class data_parser {
    parsed_data parse(const data& d);
};

class data_validator {
    bool validate(const parsed_data& pd);
};

void process_data(const data& d) {
    auto parsed = data_parser().parse(d);
    if (data_validator().validate(parsed)) {
        process(parsed);
    }
}
```

### 3. Preserve Git History

When splitting files, help preserve git history:

```bash
# Use git mv for initial split
git mv large_file.cc part1.cc
# Edit part1.cc to remove part2 content

git add part1.cc
git commit -m "Split large_file.cc: extract part1"

# Then extract part2 from original
git show HEAD^:large_file.cc > part2.cc
# Edit part2.cc to remove part1 content
git add part2.cc
git commit -m "Split large_file.cc: extract part2"
```

**Note**: This approach preserves history for part1.cc but creates part2.cc as a new file. For better history preservation, consider using `git log --follow` to track renames, or document the split in commit messages.

### 4. Update Build System

After splitting, update Makefiles:

```makefile
# Before
kernel_objects = large_file.o

# After
kernel_objects = part1.o part2.o
```

### 5. Maintain Test Coverage

Ensure tests still pass after refactoring:

```bash
./scripts/build clean
./scripts/build
./scripts/test.py
```

## Handling Automated Reviews

### Periodic Code Reviews

This project uses automated code reviews that periodically flag:
- Large files (>500 lines)
- Complex functions
- Potential code duplication
- Style inconsistencies

### Response Guidelines

When receiving an automated review:

1. **Categorize Findings**
   - External code (no action)
   - Core kernel (requires discussion)
   - Own code (potential improvements)

2. **Prioritize**
   - Focus on code that changes frequently
   - Defer work on stable, working code
   - Consider maintenance burden vs. benefit

3. **Document Decisions**
   - Why a file remains large
   - Why splitting is impractical
   - Technical debt to address later

4. **Make Incremental Improvements**
   - Fix what's easy and clear
   - Avoid massive refactoring
   - Preserve stability

### Example Response

```markdown
## Code Review Response

### Files Requiring No Action
- ZFS files: External OpenSolaris code
- BSD network stack: Upstream FreeBSD code
- ENA driver: AWS-provided driver code

### Files Under Consideration
- scripts/loader.py: Could be modularized
- tests/tst-stdio.cc: Could split by test category

### Files Accepted As-Is
- core/sched.cc: Cohesive scheduler implementation
- loader.cc: Core kernel loader, splitting impractical
```

## Enforcement

These guidelines are recommendations, not strict rules. Use judgment when applying them. Consider:

- **Stability**: Working code should not be changed without good reason
- **Maintainability**: Will the change make maintenance easier or harder?
- **External Code**: Third-party code requires special considerations
- **Team Consensus**: Discuss significant refactoring with the team

## References

- [CODINGSTYLE.md](CODINGSTYLE.md) - OSv coding style
- [BEST_PRACTICES.md](BEST_PRACTICES.md) - Development best practices
- [CODE_REVIEW.md](CODE_REVIEW.md) - Code review guidelines

---

*This document reflects the OSv project's practical approach to code organization, balancing ideal practices with the realities of kernel development and external dependencies.*
