# VirtioFS mmap Support Implementation

## Overview
This implementation adds memory-mapped file support to the virtiofs filesystem in OSv by implementing the `vnop_cache_t` operation (arc) that was previously set to `nullptr`.

## Changes Made

### 1. Added Required Include
- Added `#include <osv/mmu-defs.hh>` to access `mmu::page_size` constant

### 2. Implemented virtiofs_cache Function
```c
static int virtiofs_cache(struct vnode* vnode, struct file* fp, struct uio* uio)
```

**Function Features:**
- **Validation**: Ensures operation is on regular files only, with valid offsets and exactly page-sized reads
- **DAX Integration**: Uses existing DAX infrastructure when available for optimal performance
- **Fallback Support**: Falls back to FUSE_READ mechanism when DAX is unavailable
- **Error Handling**: Proper error codes matching VFS expectations

**Key Validations:**
- `vnode->v_type == VREG` (regular files only)
- `uio->uio_offset >= 0` (valid offset)
- `uio->uio_resid == mmu::page_size` (exactly one page)
- `uio->uio_offset < vnode->v_size` (within file bounds)

### 3. Updated vnops Structure
- Changed `virtiofs_arc` from `((vnop_cache_t) nullptr)` to `virtiofs_cache`
- Updated comment to reflect implementation completion

## How It Works

### Before Implementation
- VFS layer detected `vop_cache == nullptr`
- Fell back to `mmu::default_file_mmap()` (less efficient)
- No page caching for memory-mapped files

### After Implementation
- VFS layer detects `vop_cache != nullptr`
- Uses `mmu::map_file_mmap()` (more efficient with page caching)
- Page cache system calls `virtiofs_cache()` to read pages on demand
- DAX-enabled files get optimal performance through direct memory access
- Non-DAX files still work through FUSE_READ fallback

## Benefits

1. **Performance**: DAX-enabled files get direct memory access for mmap operations
2. **Compatibility**: Non-DAX files still work through existing fallback mechanisms
3. **Consistency**: Implementation follows existing virtiofs patterns and architecture
4. **Memory Efficiency**: Enables page cache reuse for memory-mapped files

## Technical Details

- **Function Signature**: Matches `vnop_cache_t` typedef exactly
- **Page Size**: Uses `mmu::page_size` (4096 bytes on most architectures)
- **DAX Integration**: Reuses existing `dax_manager::read()` method
- **Fallback**: Uses existing `virtiofs_read_fallback()` function
- **Thread Safety**: Inherits thread safety from underlying DAX and FUSE implementations

## Files Modified

- `fs/virtiofs/virtiofs_vnops.cc`: Added include, implemented function, updated vnops structure

## Testing Recommendations

1. Test mmap operations on DAX-enabled virtiofs mounts
2. Test mmap operations on non-DAX virtiofs mounts  
3. Verify existing file operations remain unaffected
4. Test error conditions (invalid offsets, non-regular files, etc.)
5. Performance comparison between old and new mmap implementations

## References

- Original GitHub issue: https://github.com/cloudius-systems/osv/issues/1062#issuecomment-712988712
- Code reference: https://github.com/cloudius-systems/osv/blob/5a2353cf7fd03212317f0b50178825ac10472bfb/fs/virtiofs/virtiofs_vnops.cc#L405