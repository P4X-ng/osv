# Ubuntu Package-Based OSv Modules

This document describes the new Ubuntu package-based module system for OSv, which allows you to easily create OSv images by specifying Ubuntu packages.

## Overview

The Ubuntu package system provides a simple way to create OSv modules by:

1. Specifying a list of Ubuntu packages to include
2. Automatically resolving dependencies (with blacklisting support)
3. Downloading and extracting packages
4. Applying transformations to remove unnecessary files
5. Generating OSv manifests and images

## Quick Start

### Creating a Simple Module

Use the helper script to create a new module:

```bash
./scripts/create_ubuntu_module.py --name myapp --packages curl,wget --cmdline "/usr/bin/curl --help"
```

This creates a new module in `modules/myapp/` with:
- `Makefile` - Build configuration
- `myapp.json` - Package and transformation configuration  
- `module.py` - OSv module integration
- `README.md` - Documentation

### Building the Module

```bash
./scripts/build image=myapp
```

## Manual Module Creation

### 1. Create Module Directory

```bash
mkdir modules/myapp
```

### 2. Create Makefile

Create `modules/myapp/Makefile`:

```makefile
SRC = $(shell readlink -f $(dir $(lastword $(MAKEFILE_LIST))))
OSV_ROOT = $(SRC)/../..
UBUNTU_PACKAGES_SCRIPT = $(OSV_ROOT)/scripts/ubuntu_packages.py

ARCH ?= amd64
UBUNTU_RELEASE ?= focal
MODULE_DIR = $(OSV_BUILD_PATH)/modules/myapp

.PHONY: module clean

module: $(MODULE_DIR)/usr.manifest

$(MODULE_DIR)/usr.manifest: $(SRC)/myapp.json
	@echo "Building myapp module using Ubuntu packages..."
	@mkdir -p $(MODULE_DIR)
	@$(UBUNTU_PACKAGES_SCRIPT) \
		--config $(SRC)/myapp.json \
		--output-dir $(MODULE_DIR) \
		--arch $(ARCH) \
		--ubuntu-release $(UBUNTU_RELEASE)
	@echo "$(MODULE_DIR)/usr.manifest: $(MODULE_DIR)/usr.manifest" > $(OSV_BUILD_PATH)/usr.manifest
	@if [ -f $(MODULE_DIR)/cmdline ]; then \
		cp $(MODULE_DIR)/cmdline $(OSV_BUILD_PATH)/cmdline; \
	fi

clean:
	@rm -rf $(MODULE_DIR)
```

### 3. Create Configuration File

Create `modules/myapp/myapp.json`:

```json
{
    "packages": [
        "curl",
        "libcurl4"
    ],
    "blacklist": [
        "bash",
        "systemd",
        "apt",
        "dpkg"
    ],
    "transformations": {
        "remove_files": [
            "*.a",
            "*.la"
        ],
        "remove_directories": [
            "usr/share/doc",
            "usr/share/man",
            "var/lib/dpkg"
        ]
    },
    "manifest_prefix": "/usr",
    "cmdline": "/usr/bin/curl --help"
}
```

### 4. Create Module Integration (Optional)

Create `modules/myapp/module.py`:

```python
from osv.modules import api

default = api.run('/usr/bin/curl --help')
```

## Configuration Reference

### Package Configuration

The JSON configuration file supports the following options:

#### `packages` (required)
List of Ubuntu packages to download and include:
```json
"packages": ["nginx-core", "nginx-common", "curl"]
```

#### `blacklist` (optional)
Packages to exclude from dependency resolution:
```json
"blacklist": ["systemd", "bash", "apt", "dpkg"]
```

Default blacklist includes common system packages that are not needed in OSv:
- Shell interpreters (bash, dash, etc.)
- Init systems (systemd, upstart, etc.)
- Package managers (apt, dpkg, etc.)
- System utilities (mount, util-linux, etc.)

#### `transformations` (optional)
File and directory transformations to apply:

```json
"transformations": {
    "remove_files": ["*.a", "*.la", "*.pyc"],
    "remove_directories": ["usr/share/doc", "usr/share/man"],
    "move_files": {
        "usr/bin/old-name": "usr/bin/new-name"
    },
    "create_symlinks": {
        "var/log/app": "/tmp/app-logs"
    }
}
```

#### `manifest_prefix` (optional)
Prefix for paths in the OSv manifest (default: "/usr"):
```json
"manifest_prefix": "/usr"
```

#### `cmdline` (optional)
Default command line for the application:
```json
"cmdline": "/usr/bin/myapp --config /etc/myapp.conf"
```

## Command Line Usage

The `ubuntu_packages.py` script can also be used directly:

### Basic Usage

```bash
# Download single package
./scripts/ubuntu_packages.py --packages curl --output-dir /tmp/curl

# Download multiple packages without dependencies
./scripts/ubuntu_packages.py --packages "nginx,curl,wget" --no-deps --output-dir /tmp/tools

# Use configuration file
./scripts/ubuntu_packages.py --config myapp.json --output-dir /tmp/myapp
```

### Architecture Support

```bash
# Build for aarch64
./scripts/ubuntu_packages.py --packages curl --arch arm64 --output-dir /tmp/curl-arm64

# Specify Ubuntu release
./scripts/ubuntu_packages.py --packages curl --ubuntu-release jammy --output-dir /tmp/curl-jammy
```

### Advanced Options

```bash
# Custom cache directory
./scripts/ubuntu_packages.py --packages curl --cache-dir /tmp/cache --output-dir /tmp/curl

# Custom blacklist
./scripts/ubuntu_packages.py --packages nginx --blacklist "systemd,bash,perl" --output-dir /tmp/nginx

# Verbose output
./scripts/ubuntu_packages.py --packages curl --verbose --output-dir /tmp/curl
```

## Examples

### Web Server (Nginx)

```json
{
    "packages": ["nginx-core", "nginx-common"],
    "transformations": {
        "remove_directories": ["etc/init.d", "lib/systemd"],
        "create_symlinks": {
            "var/log/nginx": "/tmp/nginx-logs",
            "var/lib/nginx": "/tmp/nginx-lib"
        }
    },
    "cmdline": "/usr/sbin/nginx -g 'daemon off;'"
}
```

### Development Tools

```json
{
    "packages": ["gcc", "make", "git", "vim"],
    "transformations": {
        "remove_directories": ["usr/share/doc", "usr/share/man"]
    }
}
```

### Database Client

```json
{
    "packages": ["mysql-client", "postgresql-client"],
    "blacklist": ["systemd", "perl-base"],
    "cmdline": "/usr/bin/mysql --help"
}
```

## Integration with Build System

### Building with scripts/build

```bash
# Build single module
./scripts/build image=myapp

# Build with specific architecture
./scripts/build arch=aarch64 image=myapp

# Build with multiple modules
./scripts/build image=myapp,nginx-ubuntu
```

### Environment Variables

The build system passes these variables to module Makefiles:
- `ARCH` - Target architecture (x64, aarch64)
- `OSV_BUILD_PATH` - Build output directory
- `UBUNTU_RELEASE` - Ubuntu release codename

## Troubleshooting

### Package Not Found

If a package cannot be found:
1. Check the package name is correct
2. Verify the Ubuntu release supports the package
3. Try a different repository section (main, universe, etc.)

### Dependency Issues

If dependency resolution fails:
1. Add problematic packages to the blacklist
2. Use `--no-deps` to disable dependency resolution
3. Manually specify required dependencies in the packages list

### Build Failures

If the module build fails:
1. Check the OSv build log for errors
2. Verify all required libraries are included
3. Check file permissions and paths in the manifest

### Performance Issues

For faster builds:
1. Use a local package cache
2. Disable dependency resolution for known package sets
3. Use package mirrors closer to your location

## Limitations

### Current Limitations

1. **Dependency Resolution**: Basic dependency resolution using apt-cache
2. **Package Conflicts**: No automatic conflict resolution
3. **Virtual Packages**: Limited support for virtual package alternatives
4. **Cross-Architecture**: Limited cross-compilation support

### Planned Improvements

1. Enhanced dependency resolution using apt solver
2. Support for multiple Ubuntu releases simultaneously
3. Package conflict detection and resolution
4. Better cross-architecture support
5. Integration with Capstan tool

## Contributing

To contribute to the Ubuntu package system:

1. Test with different package combinations
2. Report issues with specific packages
3. Suggest improvements to dependency resolution
4. Add support for additional Ubuntu releases
5. Improve documentation and examples

## See Also

- [OSv Module System](../modules/README.md)
- [Build System Documentation](../scripts/README.md)
- [Application Development Guide](../documentation/APPLICATIONS.md)