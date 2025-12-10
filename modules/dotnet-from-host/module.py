from osv.modules.filemap import FileMap
from osv.modules import api
import os, os.path
import subprocess
import sys

# This module provides .NET runtime support for OSv
# It copies .NET SDK/Runtime from the host system to the OSv image

provides = ['dotnet']

usr_files = FileMap()

# Try to find .NET installation on the host
# Common .NET installation paths on Linux:
# - /usr/share/dotnet (standard)
# - /usr/lib/dotnet (alternative)
# - ~/.dotnet (user installation)

dotnet_paths = [
    '/usr/share/dotnet',
    '/usr/lib/dotnet',
    os.path.expanduser('~/.dotnet')
]

dotnet_path = None
for path in dotnet_paths:
    if os.path.exists(path):
        dotnet_path = path
        break

if dotnet_path is None:
    # Try to find dotnet executable
    try:
        dotnet_exe = subprocess.check_output(['which', 'dotnet']).decode('utf-8').strip()
        if dotnet_exe:
            # Resolve symlinks to find actual installation
            dotnet_real_path = os.path.realpath(dotnet_exe)
            # Usually: /usr/share/dotnet/dotnet or /usr/lib/dotnet/dotnet
            dotnet_path = os.path.dirname(dotnet_real_path)
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

if dotnet_path is None:
    print('Could not find .NET installation on the host.')
    print('Please install .NET 5.0 or later from: https://dotnet.microsoft.com/download')
    print('Common installation locations:')
    for path in dotnet_paths:
        print('  - ' + path)
    sys.exit(1)

# Verify .NET version
try:
    dotnet_version = subprocess.check_output(
        [os.path.join(dotnet_path, 'dotnet'), '--version'],
        stderr=subprocess.STDOUT
    ).decode('utf-8').strip()
    print('Found .NET version: ' + dotnet_version)
    
    # Check if version is at least 5.0
    major_version = int(dotnet_version.split('.')[0])
    if major_version < 5:
        print('Warning: .NET version %s detected. Version 5.0 or later is recommended.' % dotnet_version)
except (subprocess.CalledProcessError, FileNotFoundError, ValueError) as e:
    print('Warning: Could not verify .NET version: %s' % str(e))

# Add .NET runtime files to the image
# We include:
# - dotnet executable
# - shared runtime libraries
# - host framework resolver (hostfxr)
# - host policy (hostpolicy)

usr_files.add(dotnet_path).to('/usr/lib/dotnet') \
    .include('dotnet') \
    .include('shared/**') \
    .include('host/**') \
    .exclude('shared/**/native/libcoreclrtraceptprovider.so') \
    .exclude('**/*.pdb')

# Create convenience symlink
usr_files.link('/usr/bin/dotnet').to('/usr/lib/dotnet/dotnet')
