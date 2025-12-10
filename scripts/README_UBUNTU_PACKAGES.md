# Building OSv Images from Ubuntu Packages

This guide explains how to use the `build_from_ubuntu_packages.py` script to easily create OSv images from Ubuntu packages.

## Overview

The `build_from_ubuntu_packages.py` script automates the process of:
1. Downloading Ubuntu .deb packages
2. Extracting their contents
3. Optionally resolving dependencies
4. Filtering out unnecessary files
5. Generating OSv manifest files
6. Creating module structure for use with `scripts/build`

## Prerequisites

The script requires the following tools (usually available on Ubuntu/Debian systems):
- `apt-get` and `apt-cache` for package management
- `dpkg` for package information
- `ar` and `tar` for extracting .deb files
- Python 3.6+

## Basic Usage

### Download a Single Package

```bash
./scripts/build_from_ubuntu_packages.py -o /path/to/output curl
```

This will download the `curl` package and extract it to `/path/to/output/extracted/`.

### Download with Dependencies

```bash
./scripts/build_from_ubuntu_packages.py -o /path/to/output -d nginx
```

The `-d` flag enables recursive dependency resolution. Note that some dependencies are blacklisted by default (like bash, perl, etc.) as they're not needed in OSv.

### Specify Command Line

```bash
./scripts/build_from_ubuntu_packages.py \
  -o /path/to/output \
  -c "/usr/bin/curl https://example.com" \
  --generate-module \
  curl
```

The `-c` flag specifies the command line to run in OSv, and `--generate-module` creates the necessary `module.py` and `Makefile` files.

### Custom Blacklist

```bash
./scripts/build_from_ubuntu_packages.py \
  -o /path/to/output \
  -d \
  -b "gcc.*,make,autoconf" \
  mypackage
```

The `-b` flag adds additional patterns to the blacklist (uses regex matching).

## Creating an App Module

To create a complete app module that works with the OSv build system:

```bash
# Create an app directory (can be in apps/ or modules/)
mkdir -p apps/curl-from-ubuntu

# Build the module with the script
./scripts/build_from_ubuntu_packages.py \
  -o apps/curl-from-ubuntu \
  -c "/usr/bin/curl https://example.com" \
  --generate-module \
  -d \
  curl

# Build OSv image with this app
./scripts/build image=curl-from-ubuntu
```

## Generated Structure

After running the script with `--generate-module`, your output directory will contain:

```
output/
├── downloads/          # Downloaded .deb files (cached)
├── extracted/          # Extracted package contents
│   └── usr/
│       └── bin/
│           └── curl
├── usr.manifest        # OSv manifest file
├── module.py          # OSv module definition
└── Makefile           # Makefile for scripts/build
```

## Default Blacklist

The following package patterns are blacklisted by default:
- `bash`, `dash`, `sh` - Shell interpreters (not needed)
- `perl`, `python.*` - Script interpreters (usually not needed)
- `debconf`, `dpkg` - Package management tools
- `init`, `systemd` - Init systems
- `udev` - Device management
- `coreutils` - Basic utilities (provided by OSv)
- `base-files` - Base system files

Use `--no-default-blacklist` to disable default blacklisting.

## File Cleanup

The script automatically removes common unnecessary files:
- `/usr/share/doc` - Documentation
- `/usr/share/man` - Man pages
- `/usr/share/info` - Info pages
- `/usr/share/locale` - Localization (optional, can be kept if needed)
- `/var/lib/dpkg` - Package database
- `/var/cache` - Cache files

Empty directories are also removed.

## Examples

### Example 1: Simple Utility (curl)

```bash
./scripts/build_from_ubuntu_packages.py \
  -o apps/curl-example \
  -c "/usr/bin/curl -V" \
  --generate-module \
  curl

./scripts/build image=curl-example
./scripts/run.py
```

### Example 2: Nginx Web Server

```bash
./scripts/build_from_ubuntu_packages.py \
  -o apps/nginx-from-pkg \
  -c "/usr/sbin/nginx -g 'daemon off;'" \
  --generate-module \
  -d \
  nginx

./scripts/build image=nginx-from-pkg
./scripts/run.py -nv
```

### Example 3: Multiple Related Packages

```bash
./scripts/build_from_ubuntu_packages.py \
  -o apps/openssl-tools \
  -c "/usr/bin/openssl version" \
  --generate-module \
  openssl ca-certificates
```

## Architecture Support

By default, the script downloads amd64 packages. For ARM64:

```bash
./scripts/build_from_ubuntu_packages.py \
  -o apps/myapp \
  -a arm64 \
  --generate-module \
  mypackage
```

Note: You'll need ARM64 cross-compilation tools set up for OSv.

## Troubleshooting

### Package Not Found

If apt-get can't find a package:
1. Ensure your apt cache is updated: `sudo apt-get update`
2. Check the exact package name: `apt-cache search <name>`

### Missing Dependencies

If a package doesn't work due to missing libraries:
1. Try adding `-d` to resolve dependencies automatically
2. Manually add required packages to the command line
3. Check what libraries are needed: `ldd /path/to/binary`

### File Conflicts

If files from different packages conflict, consider:
1. Choosing only the packages you need
2. Manually resolving conflicts in the extracted directory

## Integration with scripts/build

The generated module can be used just like any other OSv app:

```bash
# Build image
./scripts/build image=myapp-from-ubuntu

# Build with custom parameters
./scripts/build fs=rofs image=myapp-from-ubuntu

# Build multiple modules
./scripts/build image=httpserver,myapp-from-ubuntu
```

## Notes

- Downloaded .deb files are cached in the `downloads/` directory for faster subsequent runs
- The script works on any Linux distribution, not just Ubuntu (as long as apt tools are available)
- PIE (Position Independent Executable) binaries from Ubuntu packages work well on OSv
- The manifest uses absolute paths to the extracted files, so don't move the output directory after generation
