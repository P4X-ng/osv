# .NET Runtime Module for OSv

This module provides support for running .NET applications on OSv by copying the .NET runtime from the host system.

## Prerequisites

1. **Install .NET SDK or Runtime** on your Linux host system:
   - Download from: https://dotnet.microsoft.com/download
   - Recommended version: .NET 5.0 or later (6.0, 7.0, 8.0+)
   - For Ubuntu/Debian:
     ```bash
     wget https://packages.microsoft.com/config/ubuntu/20.04/packages-microsoft-prod.deb -O packages-microsoft-prod.deb
     sudo dpkg -i packages-microsoft-prod.deb
     sudo apt-get update
     sudo apt-get install -y dotnet-sdk-8.0
     ```
   - For Fedora/RHEL:
     ```bash
     sudo dnf install dotnet-sdk-8.0
     ```

2. **Verify .NET installation**:
   ```bash
   dotnet --version
   ```

## Building OSv Image with .NET Support

### Important: Use Symbol Hiding

.NET applications require building OSv with `conf_hide_symbols=1` to avoid conflicts between OSv's built-in C++ standard library and the one used by .NET runtime:

```bash
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host
```

### Building with Your .NET Application

1. **Create your .NET application**:
   ```bash
   # Create a simple console app
   mkdir myapp
   cd myapp
   dotnet new console
   dotnet publish -c Release -o publish --self-contained false
   ```

2. **Create a module for your app**:
   Create `modules/dotnet-hello/module.py`:
   ```python
   from osv.modules.filemap import FileMap
   from osv.modules import api
   
   api.require('dotnet-from-host')
   
   usr_files = FileMap()
   usr_files.add('${YOUR_APP_PATH}/publish').to('/app') \
       .include('**')
   
   default = '/usr/bin/dotnet /app/myapp.dll'
   ```

3. **Build OSv image**:
   ```bash
   ./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,dotnet-hello
   ```

## Running .NET Applications on OSv

### Run with QEMU

```bash
./scripts/run.py -e '/usr/bin/dotnet /app/myapp.dll'
```

### Run with Firecracker

```bash
./scripts/firecracker.py -e '/usr/bin/dotnet /app/myapp.dll'
```

## Example: Hello World

Here's a complete example to run a .NET Hello World application:

```bash
# 1. Create a simple .NET app
mkdir /tmp/dotnet-hello
cd /tmp/dotnet-hello
dotnet new console
dotnet publish -c Release -o publish --self-contained false

# 2. Create OSv module
mkdir -p modules/dotnet-hello-example
cat > modules/dotnet-hello-example/module.py << 'EOF'
from osv.modules.filemap import FileMap
from osv.modules import api

api.require('dotnet-from-host')

usr_files = FileMap()
usr_files.add('/tmp/dotnet-hello/publish').to('/app') \
    .include('**')

default = '/usr/bin/dotnet /app/dotnet-hello.dll'
EOF

cat > modules/dotnet-hello-example/Makefile << 'EOF'
.PHONY: module clean

include ../common.gmk

module:
	@echo "dotnet-hello-example module configured"

clean:
	@echo "Nothing to clean"
EOF

# 3. Build OSv image with symbol hiding enabled
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,dotnet-hello-example

# 4. Run the application
./scripts/run.py
```

## Known Limitations and Issues

### Symbol Hiding Required

As mentioned in the original OSv issue discussion (https://groups.google.com/g/osv-dev/c/k69cHw7qvTg/m/3udXnsDGBwAJ), .NET applications require `conf_hide_symbols=1` to be set when building OSv. This prevents conflicts between:
- OSv's built-in `libstdc++.so.6`
- The C++ standard library dependencies used by .NET runtime

Without `conf_hide_symbols=1`, you may encounter symbol resolution errors or crashes.

### Self-Contained vs Framework-Dependent

- **Framework-dependent** deployments are recommended (smaller, uses .NET from dotnet-from-host module)
- **Self-contained** deployments bundle the entire .NET runtime with the app (larger but may be more reliable)

For self-contained deployment:
```bash
dotnet publish -c Release -o publish --self-contained true -r linux-x64
```

### Memory Requirements

.NET applications typically require more memory than native applications. Ensure OSv is configured with adequate memory:
```bash
./scripts/run.py -m 512M -e '/usr/bin/dotnet /app/myapp.dll'
```

### Threading and Async

.NET's ThreadPool and async/await patterns should work on OSv. However, performance may differ from standard Linux due to OSv's unikernel architecture.

### Known Issues

1. **GC Performance**: The .NET garbage collector may exhibit different performance characteristics on OSv
2. **Native Dependencies**: .NET apps with native dependencies (P/Invoke) may require additional libraries to be included
3. **File I/O**: ZFS filesystem is recommended over ROFS for write-heavy .NET applications

## Advanced Configuration

### Using Custom .NET Installation

If .NET is installed in a non-standard location, set the path in module.py:

```python
dotnet_path = '/custom/path/to/dotnet'
```

### Including Additional .NET Components

Modify the FileMap in `module.py` to include additional components:

```python
usr_files.add(dotnet_path).to('/usr/lib/dotnet') \
    .include('dotnet') \
    .include('shared/**') \
    .include('host/**') \
    .include('sdk/**')  # Include SDK for runtime compilation
```

### Optimizing Image Size

For production deployments, consider:
1. Use `fs=rofs` (Read-Only FS) for smaller, faster images
2. Use framework-dependent deployment (smaller app size)
3. Trim unused assemblies with .NET's PublishTrimmed option:
   ```bash
   dotnet publish -c Release -o publish --self-contained false -p:PublishTrimmed=true
   ```

## Troubleshooting

### Error: "Could not find .NET installation"

Ensure .NET is properly installed:
```bash
which dotnet
dotnet --version
```

### Error: Symbol resolution failures

Rebuild with symbol hiding:
```bash
./scripts/build clean
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,your-app
```

### Error: Out of memory

Increase OSv memory allocation:
```bash
./scripts/run.py -m 1G -e '/usr/bin/dotnet /app/myapp.dll'
```

### Error: Application crashes on startup

Check that all required .NET runtime files are included. Try using self-contained deployment:
```bash
dotnet publish -c Release -o publish --self-contained true -r linux-x64
```

## Testing Different .NET Versions

### .NET 5.0
```bash
sudo apt-get install dotnet-sdk-5.0
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,your-app
```

### .NET 6.0 (LTS)
```bash
sudo apt-get install dotnet-sdk-6.0
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,your-app
```

### .NET 8.0 (Latest LTS)
```bash
sudo apt-get install dotnet-sdk-8.0
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,your-app
```

## Contributing

If you encounter issues or have improvements for .NET support on OSv, please report them at:
- https://github.com/cloudius-systems/osv/issues
- https://groups.google.com/forum/#!forum/osv-dev

## References

- [.NET on Linux Installation Guide](https://docs.microsoft.com/en-us/dotnet/core/install/linux)
- [OSv Google Group Discussion on .NET](https://groups.google.com/g/osv-dev/c/k69cHw7qvTg/m/3udXnsDGBwAJ)
- [.NET Runtime GitHub Issue #13790](https://github.com/dotnet/runtime/issues/13790)
- [OSv Symbol Hiding Documentation](../../documentation/KERNEL_CUSTOMIZATION_GUIDE.md#symbol-hiding-size-reduction)
