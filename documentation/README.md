# OSv Documentation Index

## Overview

This directory contains comprehensive documentation for the OSv unikernel project, covering architecture, customization, integration, best practices, and troubleshooting.

## Documentation Structure

### 1. [Comprehensive Code Review](COMPREHENSIVE_CODE_REVIEW.md)
**Purpose:** In-depth analysis of OSv architecture, components, and implementation

**Contents:**
- Architecture overview and component analysis
- Memory management and threading
- Networking stack implementation
- Filesystem layer (ZFS, ROFS, RAMFS, VirtioFS)
- Device drivers and hardware abstraction
- Build system and modularization
- Security analysis
- Performance evaluation
- Code quality assessment
- Comparison with upstream cloudius-systems/osv

**Audience:** Developers, architects, contributors, researchers

**Use When:**
- Understanding OSv internals
- Contributing to OSv development
- Evaluating OSv for your project
- Researching unikernel architecture

---

### 2. [Kernel Customization Guide](KERNEL_CUSTOMIZATION_GUIDE.md)
**Purpose:** Comprehensive guide to tailoring OSv kernel for specific needs

**Contents:**
- Build configuration options
- Module selection and dependencies
- Kernel size reduction strategies (from 6.8MB to <3MB)
- Filesystem selection criteria
- Architecture-specific optimizations (x86_64, aarch64)
- Performance tuning techniques
- Advanced customization (custom boot parameters, drivers, filesystems)
- Configuration examples and templates

**Audience:** System administrators, DevOps engineers, performance engineers

**Use When:**
- Optimizing kernel size for serverless/containers
- Tuning for specific hypervisors (QEMU, Firecracker, VMware)
- Reducing boot time (<10ms target)
- Minimizing memory footprint
- Selecting appropriate filesystem

---

### 3. [Custom Stack Integration Guide](CUSTOM_STACK_INTEGRATION.md)
**Purpose:** Guide for replacing or extending OSv core components

**Contents:**
- Custom networking stack integration (DPDK, mTCP examples)
- Custom ELF loader implementation (JIT, plugins)
- Custom filesystem development (memfs, COW, caching)
- Custom device driver framework
- Custom memory allocators (slab allocator example)
- Integration patterns and best practices
- Testing and validation strategies

**Audience:** Advanced developers, researchers, systems programmers

**Use When:**
- Integrating specialized networking stacks
- Implementing custom filesystems
- Adding new device drivers
- Research and experimentation
- Integrating proprietary components

---

### 4. [Best Practices and Common Pitfalls](BEST_PRACTICES_AND_PITFALLS.md)
**Purpose:** Practical guide based on community experience and upstream issues

**Contents:**
- Application porting strategies (Java, Python, Go, Node.js, Rust)
- Performance optimization techniques
  - Boot time optimization (<10ms)
  - Memory optimization
  - Network and disk I/O tuning
- Security considerations and best practices
- Common pitfalls and solutions
  - Build issues
  - Runtime issues
  - Application compatibility
- Debugging strategies (GDB, tracing, profiling)
- Production deployment checklist
- Troubleshooting guide

**Audience:** Developers, operations engineers, application architects

**Use When:**
- Porting applications to OSv
- Troubleshooting build or runtime issues
- Optimizing performance
- Preparing for production deployment
- Learning from community experience

**Key Insights from Upstream:**
- Issue #450: VFS locking impacts disk I/O
- Issue #1013: Memory pool tuning needed
- Issue #987: VGA console adds 60-70ms to boot
- No fork/exec support
- Minimum 11MB RAM requirement

---

### 5. [.NET Runtime Support Guide](DOTNET_SUPPORT.md)
**Purpose:** Comprehensive guide for running .NET applications on OSv

**Contents:**
- Prerequisites and .NET installation
- Symbol hiding requirement (conf_hide_symbols=1) explanation
- Quick start guide with examples
- Deployment strategies (framework-dependent, self-contained, trimmed)
- ASP.NET Core web application support
- Console applications and worker services
- Performance tuning for .NET on OSv
- Known issues and limitations
- Comprehensive troubleshooting guide
- Multiple real-world examples

**Audience:** .NET developers, DevOps engineers, cloud architects

**Use When:**
- Running .NET Core/5+ applications on OSv
- Porting ASP.NET Core web services
- Understanding C++ library conflict resolution
- Troubleshooting .NET runtime issues
- Optimizing .NET performance on OSv

**Key Requirements:**
- Must use `conf_hide_symbols=1` to avoid libstdc++ conflicts
- Recommended: .NET 5.0 or later (6.0, 8.0 LTS preferred)
- Framework-dependent deployment recommended for smaller images
- Minimum 512MB RAM for typical .NET applications

---

### 6. [Quick Reference Guide](QUICK_REFERENCE.md)
**Purpose:** Cheat sheet for common commands and configurations

**Contents:**
- Build commands reference
- Run commands and boot parameters
- Configuration options table
- Filesystem commands
- Debugging commands (GDB, tracing)
- Module management
- Performance tuning quick tips
- Size optimization checklist
- Language-specific examples
- Cloud deployment commands
- Troubleshooting one-liners

**Audience:** All users (quick reference)

**Use When:**
- Looking up syntax for commands
- Quick reference during development
- Finding specific configuration options
- Copying command templates

---

## Quick Start

### New to OSv?

1. **Start with:** [README.md](../README.md) for project overview
2. **Then read:** Sections 1-3 of [Best Practices and Common Pitfalls](BEST_PRACTICES_AND_PITFALLS.md#2-application-porting-best-practices)
3. **Keep handy:** [Quick Reference Guide](QUICK_REFERENCE.md)

### Building Your First Image?

1. **Read:** [Kernel Customization Guide - Section 2](KERNEL_CUSTOMIZATION_GUIDE.md#2-build-configuration-options)
2. **Try:** Examples in [Quick Reference](QUICK_REFERENCE.md#build-commands)
3. **Troubleshoot with:** [Best Practices - Section 5](BEST_PRACTICES_AND_PITFALLS.md#5-common-pitfalls-and-solutions)

### Optimizing Performance?

1. **Profile first:** [Best Practices - Section 3](BEST_PRACTICES_AND_PITFALLS.md#3-performance-optimization)
2. **Configure:** [Kernel Customization - Section 7](KERNEL_CUSTOMIZATION_GUIDE.md#7-performance-tuning)
3. **Monitor:** [Quick Reference - Monitoring](QUICK_REFERENCE.md#monitoring)

### Porting an Application?

1. **Check compatibility:** [Best Practices - Section 2.1](BEST_PRACTICES_AND_PITFALLS.md#21-assessing-application-compatibility)
2. **Follow language guide:** [Best Practices - Section 2.3](BEST_PRACTICES_AND_PITFALLS.md#23-language-specific-guidelines)
3. **For .NET apps:** [.NET Support Guide](DOTNET_SUPPORT.md)
4. **Refer to examples:** [Quick Reference - Language-Specific](QUICK_REFERENCE.md#language-specific)

### Advanced Customization?

1. **Study architecture:** [Code Review - Sections 1-7](COMPREHENSIVE_CODE_REVIEW.md)
2. **Follow patterns:** [Custom Stack Integration](CUSTOM_STACK_INTEGRATION.md)
3. **Test thoroughly:** [Custom Stack Integration - Section 8](CUSTOM_STACK_INTEGRATION.md#8-testing-and-validation)

---

## Use Case Matrix

| Use Case | Primary Doc | Supporting Docs |
|----------|-------------|-----------------|
| First-time user | Best Practices (Introduction & App Porting) | Quick Reference |
| Application porting | Best Practices (App Porting) | Quick Reference (language section) |
| .NET application porting | .NET Support Guide | Best Practices (App Porting) |
| Performance tuning | Best Practices (Performance) | Customization (Performance Tuning) |
| Size optimization | Customization (Size Reduction) | Quick Reference (optimization) |
| Boot time optimization | Best Practices (Boot Time) | Customization (Boot Optimization) |
| Production deployment | Best Practices (Production) | Quick Reference (cloud) |
| Debugging issues | Best Practices (Debugging) | Quick Reference (debugging) |
| Security review | Code Review (Security) | Best Practices (Security) |
| Architecture research | Code Review | Custom Integration |
| Custom driver development | Custom Integration (Drivers) | Code Review (Drivers) |
| Custom filesystem | Custom Integration (Filesystems) | Code Review (Filesystems) |
| Custom networking | Custom Integration (Networking) | Code Review (Networking) |
| Contributing to OSv | Code Review | Best Practices (Introduction) |

---

## Key Topics Cross-Reference

### Boot Time Optimization

- **Theory:** [Code Review §9.3](COMPREHENSIVE_CODE_REVIEW.md#93-boot-time)
- **Configuration:** [Customization §7.5](KERNEL_CUSTOMIZATION_GUIDE.md#75-boot-time-optimization)
- **Practice:** [Best Practices §3.1](BEST_PRACTICES_AND_PITFALLS.md#31-boot-time-optimization)
- **Commands:** [Quick Reference](QUICK_REFERENCE.md#boot-time-optimization)

### Memory Management

- **Architecture:** [Code Review §2](COMPREHENSIVE_CODE_REVIEW.md#2-memory-management)
- **Tuning:** [Customization §7.1](KERNEL_CUSTOMIZATION_GUIDE.md#71-memory-tuning)
- **Custom allocator:** [Custom Integration §6](CUSTOM_STACK_INTEGRATION.md#6-custom-memory-allocator)
- **Issues:** [Best Practices §5.2](BEST_PRACTICES_AND_PITFALLS.md#52-runtime-issues)

### Networking

- **Implementation:** [Code Review §4](COMPREHENSIVE_CODE_REVIEW.md#4-networking-stack)
- **Tuning:** [Customization §7.3](KERNEL_CUSTOMIZATION_GUIDE.md#73-network-tuning)
- **Custom stack:** [Custom Integration §2](CUSTOM_STACK_INTEGRATION.md#2-custom-networking-stack)
- **Troubleshooting:** [Best Practices §5.2](BEST_PRACTICES_AND_PITFALLS.md#52-runtime-issues)

### Filesystems

- **Overview:** [Code Review §5](COMPREHENSIVE_CODE_REVIEW.md#5-filesystem-layer)
- **Selection:** [Customization §5](KERNEL_CUSTOMIZATION_GUIDE.md#5-filesystem-selection)
- **Custom FS:** [Custom Integration §4](CUSTOM_STACK_INTEGRATION.md#4-custom-filesystem-implementation)
- **Commands:** [Quick Reference](QUICK_REFERENCE.md#filesystem-commands)

### Security

- **Analysis:** [Code Review §8](COMPREHENSIVE_CODE_REVIEW.md#8-security-analysis)
- **Best Practices:** [Best Practices §4](BEST_PRACTICES_AND_PITFALLS.md#4-security-considerations)
- **Production:** [Best Practices §7](BEST_PRACTICES_AND_PITFALLS.md#7-production-deployment)

---

## Known Issues and Limitations

### From Upstream (cloudius-systems/osv)

1. **VFS Locking (Issue #450)**
   - **Impact:** Disk I/O performance bottleneck
   - **Workaround:** Use ROFS, VirtioFS, or minimize concurrent access
   - **Details:** [Code Review §5.1](COMPREHENSIVE_CODE_REVIEW.md#51-virtual-filesystem-vfs), [Best Practices §5.2](BEST_PRACTICES_AND_PITFALLS.md#52-runtime-issues)

2. **Memory Pool Tuning (Issue #1013)**
   - **Impact:** Sub-optimal memory utilization
   - **Workaround:** Manual tuning via conf options
   - **Details:** [Code Review §2.1](COMPREHENSIVE_CODE_REVIEW.md#21-page-allocator), [Customization §7.1](KERNEL_CUSTOMIZATION_GUIDE.md#71-memory-tuning)

3. **VGA Console Slowness (Issue #987)**
   - **Impact:** Adds 60-70ms to boot time
   - **Workaround:** Use `--console=serial`
   - **Details:** [Best Practices §3.1](BEST_PRACTICES_AND_PITFALLS.md#31-boot-time-optimization)

4. **No fork/exec Support**
   - **Impact:** Multi-process applications won't work
   - **Workaround:** Rewrite to use threads
   - **Details:** [Best Practices §2.2](BEST_PRACTICES_AND_PITFALLS.md#22-porting-strategies)

5. **High Memory Minimum (11MB)**
   - **Impact:** Higher than some other unikernels
   - **Mitigation:** Use conf_lazy_stack, tune pools
   - **Details:** [Code Review §9.2](COMPREHENSIVE_CODE_REVIEW.md#92-memory-utilization), [Customization §7.1](KERNEL_CUSTOMIZATION_GUIDE.md#71-memory-tuning)

---

## Contributing to Documentation

### Updating Documentation

When updating OSv code that affects documentation:

1. Update relevant sections in appropriate documents
2. Cross-check references in other documents
3. Update this index if adding new sections
4. Test all code examples
5. Update version numbers if applicable

### Documentation Style

- Use Markdown formatting
- Include code examples with syntax highlighting
- Provide cross-references to related sections
- Keep examples practical and tested
- Include both "what" and "why"
- Reference upstream issues by number

### Reporting Documentation Issues

If you find errors or omissions:

1. Check if the issue exists in latest version
2. Search existing issues
3. Open an issue with:
   - Document name and section
   - Description of problem
   - Suggested correction (if any)

---

## Additional Resources

### OSv Project Resources

- **Main Repository:** https://github.com/cloudius-systems/osv
- **Wiki:** https://github.com/cloudius-systems/osv/wiki
- **Blog:** http://blog.osv.io/
- **Website:** http://osv.io/

### Community

- **Mailing List:** https://groups.google.com/forum/#!forum/osv-dev
- **Twitter:** https://twitter.com/osv_unikernel
- **Issues:** https://github.com/cloudius-systems/osv/issues

### Related Projects

- **OSv Apps:** https://github.com/cloudius-systems/osv-apps
- **Capstan:** https://github.com/cloudius-systems/capstan
- **Nightly Builds:** https://github.com/osvunikernel/osv-nightly-releases

### External Documentation

- **USENIX Paper:** https://www.usenix.org/conference/atc14/technical-sessions/presentation/kivity
- **FOSDEM Talks:** https://fosdem.org/ (search "OSv")
- **Research Papers:** See [Code Review Appendix B](COMPREHENSIVE_CODE_REVIEW.md#appendix-b-references)

---

## Document Versions

| Document | Last Updated | Version | Status |
|----------|--------------|---------|--------|
| Index (this file) | 2024-12 | 1.0 | Complete |
| Code Review | 2024-12 | 1.0 | Complete |
| Customization Guide | 2024-12 | 1.0 | Complete |
| Custom Integration | 2024-12 | 1.0 | Complete |
| Best Practices | 2024-12 | 1.0 | Complete |
| .NET Support Guide | 2024-12 | 1.0 | Complete |
| Quick Reference | 2024-12 | 1.0 | Complete |

---

## Feedback

We welcome feedback on documentation! Please:

- Open issues for errors or unclear sections
- Suggest improvements via pull requests
- Ask questions on the mailing list
- Share your OSv experiences

**Contact:** osv-dev@googlegroups.com

---

## License

This documentation is part of the OSv project and is licensed under the same terms as OSv (BSD 3-Clause License). See [LICENSE](../LICENSE) file in the repository root.
