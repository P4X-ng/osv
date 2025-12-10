#!/usr/bin/env python3

"""
Build OSv application from Ubuntu packages

This script helps create OSv images from Ubuntu packages by:
1. Downloading specified Ubuntu .deb packages
2. Extracting their contents
3. Optionally resolving dependencies
4. Filtering out unnecessary files
5. Generating OSv manifest files
"""

import os
import sys
import argparse
import subprocess
import tempfile
import shutil
import re
from pathlib import Path

# Default packages to blacklist (we don't want these in OSv)
DEFAULT_BLACKLIST = [
    'bash', 'dash', 'sh',  # Shell scripts not needed
    'perl', 'python.*',     # Usually script interpreters not needed
    'debconf', 'dpkg',      # Package management tools
    'init', 'systemd',      # Init systems
    'udev',                 # Device management
    'coreutils',            # Basic utilities we have in OSv
    'base-files',           # Base system files
]


class UbuntuPackageBuilder:
    """Builds OSv applications from Ubuntu packages"""
    
    def __init__(self, packages, output_dir, resolve_deps=False, 
                 blacklist=None, command_line=None, arch='amd64'):
        self.packages = packages
        self.output_dir = Path(output_dir).resolve()
        self.resolve_deps = resolve_deps
        self.blacklist = blacklist or DEFAULT_BLACKLIST
        self.command_line = command_line
        self.arch = arch
        self.download_dir = self.output_dir / 'downloads'
        self.extract_dir = self.output_dir / 'extracted'
        
    def run(self):
        """Main execution flow"""
        print(f"Building OSv application from Ubuntu packages: {', '.join(self.packages)}")
        
        # Create directories
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.download_dir.mkdir(parents=True, exist_ok=True)
        self.extract_dir.mkdir(parents=True, exist_ok=True)
        
        # Get list of packages to download (with dependencies if requested)
        packages_to_fetch = self._resolve_package_list()
        
        # Download packages
        downloaded_files = self._download_packages(packages_to_fetch)
        
        # Extract packages
        self._extract_packages(downloaded_files)
        
        # Clean up unwanted files
        self._cleanup_directory()
        
        # Generate manifest
        manifest_path = self._generate_manifest()
        
        print(f"\nSuccess! Package contents extracted to: {self.extract_dir}")
        print(f"Manifest generated at: {manifest_path}")
        
        return manifest_path
    
    def _resolve_package_list(self):
        """Get list of packages including dependencies if requested"""
        packages = set(self.packages)
        
        if self.resolve_deps:
            print("Resolving dependencies...")
            
            # Use apt-cache to get dependencies
            for pkg in list(packages):
                deps = self._get_package_dependencies(pkg)
                # Filter out blacklisted packages
                filtered_deps = [d for d in deps if not self._is_blacklisted(d)]
                packages.update(filtered_deps)
                
            print(f"Total packages after dependency resolution: {len(packages)}")
        
        return list(packages)
    
    def _get_package_dependencies(self, package):
        """Get dependencies for a package"""
        try:
            result = subprocess.run(
                ['apt-cache', 'depends', '--recurse', '--no-recommends', 
                 '--no-suggests', '--no-conflicts', '--no-breaks', 
                 '--no-replaces', '--no-enhances', package],
                capture_output=True, text=True, check=True
            )
            
            # Parse the output to extract package names
            deps = []
            for line in result.stdout.split('\n'):
                # Look for lines with "Depends:" 
                if 'Depends:' in line:
                    # Extract package name
                    match = re.search(r'Depends:\s+(\S+)', line)
                    if match:
                        deps.append(match.group(1))
            
            return deps
        except subprocess.CalledProcessError as e:
            print(f"Warning: Could not resolve dependencies for {package}: {e}")
            return []
    
    def _is_blacklisted(self, package):
        """Check if package is blacklisted"""
        for pattern in self.blacklist:
            if re.search(pattern, package):
                return True
        return False
    
    def _download_packages(self, packages):
        """Download .deb packages"""
        print(f"\nDownloading {len(packages)} package(s)...")
        
        downloaded_files = []
        
        for pkg in packages:
            print(f"  Downloading {pkg}...")
            
            # Check if already downloaded
            existing_debs = list(self.download_dir.glob(f"{pkg}_*.deb"))
            if existing_debs:
                print(f"    Already downloaded: {existing_debs[0].name}")
                downloaded_files.append(existing_debs[0])
                continue
            
            try:
                # Use apt-get download to get the .deb file
                result = subprocess.run(
                    ['apt-get', 'download', pkg],
                    cwd=str(self.download_dir),
                    capture_output=True,
                    text=True,
                    check=True
                )
                
                # Find the downloaded file
                new_debs = list(self.download_dir.glob(f"{pkg}_*.deb"))
                if new_debs:
                    downloaded_files.append(new_debs[0])
                    print(f"    Downloaded: {new_debs[0].name}")
                else:
                    print(f"    Warning: Could not find downloaded file for {pkg}")
                    
            except subprocess.CalledProcessError as e:
                error_msg = e.stderr if e.stderr else 'Unknown error'
                print(f"    Warning: Could not download {pkg}: {error_msg}")
                continue
        
        return downloaded_files
    
    def _extract_packages(self, deb_files):
        """Extract .deb packages to the extraction directory"""
        print(f"\nExtracting {len(deb_files)} package(s)...")
        
        for deb_file in deb_files:
            print(f"  Extracting {deb_file.name}...")
            
            # Create temp directory for this package
            with tempfile.TemporaryDirectory() as tmpdir:
                tmpdir = Path(tmpdir)
                
                # Extract the .deb file (which is an ar archive)
                subprocess.run(['ar', 'x', str(deb_file)], cwd=str(tmpdir), check=True)
                
                # Find the data archive (data.tar.* - could be .gz, .xz, .zst, etc.)
                data_archives = list(tmpdir.glob('data.tar.*'))
                if not data_archives:
                    print(f"    Warning: No data archive found in {deb_file.name}")
                    continue
                
                data_archive = data_archives[0]
                
                # Extract the data archive to our extract directory
                subprocess.run(
                    ['tar', '-xf', str(data_archive), '-C', str(self.extract_dir)],
                    check=True
                )
                
                print(f"    Extracted to {self.extract_dir}")
    
    def _cleanup_directory(self):
        """Remove unnecessary files from extracted directory"""
        print("\nCleaning up unnecessary files...")
        
        # Common directories to remove
        dirs_to_remove = [
            'usr/share/doc',
            'usr/share/man',
            'usr/share/info',
            'usr/share/locale',  # Can be optionally kept
            'var/lib/dpkg',
            'var/cache',
        ]
        
        for dir_path in dirs_to_remove:
            full_path = self.extract_dir / dir_path
            if full_path.exists():
                print(f"  Removing {dir_path}")
                shutil.rmtree(full_path)
        
        # Remove empty directories
        self._remove_empty_dirs(self.extract_dir)
    
    def _remove_empty_dirs(self, path):
        """Recursively remove empty directories"""
        try:
            for item in path.iterdir():
                if item.is_dir():
                    self._remove_empty_dirs(item)
                    try:
                        if not any(item.iterdir()):
                            item.rmdir()
                    except FileNotFoundError:
                        # Directory already removed by another recursion
                        pass
        except FileNotFoundError:
            # Path was removed during recursion
            pass
    
    def _generate_manifest(self):
        """Generate OSv manifest file"""
        manifest_path = self.output_dir / 'usr.manifest'
        
        print(f"\nGenerating manifest: {manifest_path}")
        
        with open(manifest_path, 'w') as f:
            f.write('[manifest]\n')
            
            # Walk through extracted directory and add files to manifest
            for root, dirs, files in os.walk(self.extract_dir):
                for file in files:
                    src_path = Path(root) / file
                    # Get relative path from extract_dir
                    rel_path = src_path.relative_to(self.extract_dir)
                    
                    # Guest path is the relative path with leading /
                    guest_path = '/' + str(rel_path)
                    
                    # Host path relative to module directory using ${MODULE_DIR} variable
                    # This makes the manifest portable across different machines
                    host_path_rel = Path('extracted') / rel_path
                    # Use f-string with escaped braces for MODULE_DIR variable
                    host_path = f'${{MODULE_DIR}}/{host_path_rel}'
                    
                    # Manifest format: guest_path: host_path
                    f.write(f'{guest_path}: {host_path}\n')
        
        return manifest_path
    
    def generate_module_files(self):
        """Generate module.py and Makefile for use with OSv build system"""
        
        # Generate module.py
        module_py_path = self.output_dir / 'module.py'
        with open(module_py_path, 'w') as f:
            f.write('from osv.modules import api\n\n')
            if self.command_line:
                f.write(f"default = api.run(cmdline='{self.command_line}')\n")
            else:
                f.write("# No default command line specified\n")
        
        print(f"Generated module.py at: {module_py_path}")
        
        # Generate Makefile
        makefile_path = self.output_dir / 'Makefile'
        with open(makefile_path, 'w') as f:
            f.write('.PHONY: module clean\n\n')
            f.write('module:\n')
            f.write('\t@echo "Module already built from Ubuntu packages"\n\n')
            f.write('clean:\n')
            f.write('\t@echo "Nothing to clean"\n')
        
        print(f"Generated Makefile at: {makefile_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Build OSv application from Ubuntu packages',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Download curl package only
  %(prog)s -o /tmp/curl-app curl
  
  # Download nginx with dependencies
  %(prog)s -o /tmp/nginx-app -d nginx
  
  # Download with custom blacklist
  %(prog)s -o /tmp/app -d -b bash,perl openssl
  
  # Download and set command line
  %(prog)s -o /tmp/curl-app -c "/usr/bin/curl http://example.com" curl libcurl4
        '''
    )
    
    parser.add_argument('packages', nargs='+',
                        help='Ubuntu package names to download')
    parser.add_argument('-o', '--output-dir', required=True,
                        help='Output directory for the module')
    parser.add_argument('-d', '--resolve-deps', action='store_true',
                        help='Recursively resolve and download dependencies')
    parser.add_argument('-b', '--blacklist', 
                        help='Comma-separated list of package patterns to blacklist (in addition to defaults)')
    parser.add_argument('-c', '--command-line',
                        help='Command line to run in OSv')
    parser.add_argument('-a', '--arch', default='amd64',
                        help='Architecture (default: amd64)')
    parser.add_argument('--no-default-blacklist', action='store_true',
                        help='Do not use default blacklist')
    parser.add_argument('--generate-module', action='store_true',
                        help='Generate module.py and Makefile for OSv build system')
    
    args = parser.parse_args()
    
    # Build blacklist
    blacklist = [] if args.no_default_blacklist else DEFAULT_BLACKLIST.copy()
    if args.blacklist:
        blacklist.extend(args.blacklist.split(','))
    
    # Create builder
    builder = UbuntuPackageBuilder(
        packages=args.packages,
        output_dir=args.output_dir,
        resolve_deps=args.resolve_deps,
        blacklist=blacklist,
        command_line=args.command_line,
        arch=args.arch
    )
    
    # Run the build
    try:
        builder.run()
        
        # Generate module files if requested
        if args.generate_module:
            builder.generate_module_files()
        
        print("\n✓ Build complete!")
        return 0
        
    except Exception as e:
        print(f"\n✗ Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
