# Changes Made to Fix grep -P Issue in OSv Build System

## Problem
Multiple scripts in the OSv build system were using `grep -P` (Perl-compatible regular expressions) which caused build failures on systems where grep was compiled without Perl regex support:

```
grep: support for the -P option is not compiled into this --disable-perl-regexp binary
```

The error occurred during the lua module build process, specifically when calling manifest_from_host.sh.

## Solution
Replaced all instances of `grep -P` with equivalent standard grep patterns in the affected build scripts.

### Files Modified:

#### 1. `/workspace/scripts/manifest_from_host.sh`
**Changes Made:**

1. **Simple Pattern Matching (Lines 46, 55, 58, 91, 93)**
   - Changed `grep -P "$pattern"` to `grep -E "$pattern"`
   - Changed `grep -P 'LSB shared object|LSB.*executable'` to `grep -E 'LSB shared object|LSB.*executable'`
   - Changed `grep -P 'LSB shared object'` to `grep -E 'LSB shared object'`

2. **Complex Extraction Patterns (Lines 59, 60)**
   - **Before**: `grep -Po 'lib[^ ]+.+?(?= \()'` (used Perl positive lookahead)
   - **After**: `awk '{print $1}'` (extracts first field - library name)
   
   - **Before**: `grep -Po '(?<=> )/[^ ]+'` (used Perl positive lookbehind)  
   - **After**: `awk '{print $NF}'` (extracts last field - library path)

3. **Character Class Replacements (Lines 78, 82)**
   - Changed `grep -Pv 'lib(...)\.so([\d.]+)?'` to `grep -Ev 'lib(...)\.so([0-9.]+)?'`
   - Replaced Perl-specific `\d` with POSIX character class `[0-9]`
   - Changed `-Pv` to `-Ev` for extended regex with inverted match

#### 2. `/workspace/modules/cli/rpmbuild/Makefile`
**Changes Made:**

1. **Library Path Extraction (Line 8)**
   - **Before**: `LUA_LIB_PATH = $(shell ldconfig -p | grep -Po "/.*liblua*.5\.3.so" | head -1)`
   - **After**: `LUA_LIB_PATH = $(shell ldconfig -p | grep "/.*liblua.*5\.3\.so" | head -1 | awk '{print $$NF}')`
   - Replaced Perl regex extraction with awk field parsing

## Root Cause Analysis:
The build failure occurred because:
1. The lua module build depends on the cli module
2. The cli module's Makefile calls `manifest_from_host.sh` to generate usr.manifest
3. The manifest_from_host.sh script used multiple `grep -P` patterns
4. The cli/rpmbuild/Makefile also used `grep -P` for library path detection

## Verification:
- All instances of `grep -P` have been replaced in the affected build scripts
- Script syntax is valid (no bash syntax errors)
- Functionality should remain identical - same library detection and manifest generation

## Impact:
These changes allow the build process to work on systems where grep is compiled without Perl regex support, resolving the build failure that prevented the lua module from building successfully.