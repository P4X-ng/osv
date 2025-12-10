# .NET Example Application Module

This is a template module that demonstrates how to package a .NET application for OSv.

## How to Use This Template

1. **Create your .NET application**:
   ```bash
   mkdir myapp
   cd myapp
   dotnet new console  # or web, or any other template
   # ... develop your application ...
   ```

2. **Publish your application**:
   ```bash
   dotnet publish -c Release -o publish --self-contained false
   ```

3. **Copy this template module**:
   ```bash
   cp -r modules/dotnet-example modules/my-dotnet-app
   ```

4. **Update `modules/my-dotnet-app/module.py`**:
   ```python
   from osv.modules.filemap import FileMap
   from osv.modules import api
   
   api.require('dotnet-from-host')
   
   usr_files = FileMap()
   usr_files.add('/path/to/myapp/publish').to('/app') \
       .include('**')
   
   default = '/usr/bin/dotnet /app/myapp.dll'
   ```

5. **Build OSv image** (with symbol hiding enabled):
   ```bash
   ./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,my-dotnet-app
   ```

6. **Run your application**:
   ```bash
   ./scripts/run.py
   ```

## Example: Complete Workflow

```bash
# Create a simple "Hello World" .NET console app
mkdir /tmp/hello-dotnet
cd /tmp/hello-dotnet
dotnet new console
dotnet publish -c Release -o publish --self-contained false

# Create OSv module for the app
mkdir -p modules/hello-dotnet
cat > modules/hello-dotnet/module.py << 'EOF'
from osv.modules.filemap import FileMap
from osv.modules import api

api.require('dotnet-from-host')

usr_files = FileMap()
usr_files.add('/tmp/hello-dotnet/publish').to('/app') \
    .include('**')

default = '/usr/bin/dotnet /app/hello-dotnet.dll'
EOF

cat > modules/hello-dotnet/Makefile << 'EOF'
.PHONY: module clean
include ../common.gmk
module:
	@echo "hello-dotnet module ready"
clean:
	@echo "Nothing to clean"
EOF

# Build and run
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,hello-dotnet
./scripts/run.py
```

## Tips

- Always use `conf_hide_symbols=1` when building with .NET
- Use `fs=rofs` for smaller, faster boot times
- For production apps, consider self-contained deployment
- Increase memory if needed: `./scripts/run.py -m 512M`

See the main [dotnet-from-host README](../dotnet-from-host/README.md) for more details.
