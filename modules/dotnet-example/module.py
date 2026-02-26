from osv.modules.filemap import FileMap
from osv.modules import api

# This is an example module showing how to package a .NET application for OSv
# It requires the dotnet-from-host module to provide the .NET runtime

api.require('dotnet-from-host')

# Note: This is a template/example module
# To use it, you need to:
# 1. Create your .NET application
# 2. Publish it: dotnet publish -c Release -o publish --self-contained false
# 3. Update the paths below to point to your published application
# 4. Build OSv with: ./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,dotnet-example

usr_files = FileMap()

# Example: Add your published .NET application to /app directory
# Uncomment and modify the path to your actual application:
# usr_files.add('/path/to/your/app/publish').to('/app') \
#     .include('**')

# Set the default command to run your application
# Format: '/usr/bin/dotnet /app/YourApp.dll'
# Uncomment and modify for your application:
# default = '/usr/bin/dotnet /app/YourApp.dll'
