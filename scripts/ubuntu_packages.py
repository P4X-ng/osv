#!/usr/bin/env python3
"""
Ubuntu Package Handler for OSv

This script provides functionality to download, extract, and process Ubuntu packages
for inclusion in OSv images. It supports dependency resolution, file transformations,
and integration with the OSv build system.

Usage:
    ubuntu_packages.py --packages pkg1,pkg2,... --output-dir /path/to/output [options]
"""

import argparse
import os
import sys
import subprocess
import tempfile
import shutil
import json
import re
import urllib.request
import urllib.parse
from pathlib import Path
from typing import List, Dict, Set, Optional, Tuple
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)

class UbuntuPackageError(Exception):
    """Custom exception for Ubuntu package handling errors"""
    pass

class UbuntuPackageHandler:
    """Main class for handling Ubuntu package operations"""
    
    def __init__(self, arch: str = 'amd64', ubuntu_release: str = 'focal', 
                 cache_dir: Optional[str] = None, verbose: bool = False):
        self.arch = arch
        self.ubuntu_release = ubuntu_release
        self.verbose = verbose
        
        if verbose:
            logger.setLevel(logging.DEBUG)
            
        # Set up cache directory
        if cache_dir:
            self.cache_dir = Path(cache_dir)
        else:
            osv_root = Path(__file__).parent.parent
            self.cache_dir = osv_root / 'build' / 'downloaded_packages' / 'ubuntu' / ubuntu_release / arch
        
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.packages_cache_dir = self.cache_dir / 'packages'
        self.packages_cache_dir.mkdir(exist_ok=True)
        
        # Package repositories
        if arch == 'amd64':
            self.repo_base = 'http://archive.ubuntu.com/ubuntu'
        else:
            self.repo_base = 'http://ports.ubuntu.com/ubuntu-ports'
            
        # Default blacklist for dependencies
        self.default_blacklist = {
            'bash', 'dash', 'sh', 'csh', 'tcsh', 'zsh',  # shells
            'systemd', 'systemd-sysv', 'init', 'upstart',  # init systems
            'udev', 'systemd-udev',  # device management
            'dbus', 'dbus-x11',  # message bus
            'apt', 'dpkg', 'debconf',  # package management
            'perl-base', 'python3-minimal',  # interpreters (unless explicitly requested)
            'mount', 'util-linux',  # system utilities
            'coreutils', 'findutils', 'grep', 'sed',  # basic utilities
        }
        
    def get_package_info(self, package_name: str) -> Optional[Dict]:
        """Get package information from Ubuntu repositories"""
        try:
            # Try to get package info using apt-cache if available
            if shutil.which('apt-cache'):
                result = subprocess.run(['apt-cache', 'show', package_name], 
                                      capture_output=True, text=True, check=False)
                if result.returncode == 0:
                    return self._parse_apt_cache_output(result.stdout)
            
            # Fallback to web scraping package information
            return self._get_package_info_web(package_name)
            
        except Exception as e:
            logger.warning(f"Could not get package info for {package_name}: {e}")
            return None
    
    def _parse_apt_cache_output(self, output: str) -> Dict:
        """Parse apt-cache show output"""
        info = {}
        for line in output.split('\n'):
            if ':' in line:
                key, value = line.split(':', 1)
                info[key.strip()] = value.strip()
        return info
    
    def _get_package_info_web(self, package_name: str) -> Optional[Dict]:
        """Get package information by scraping Ubuntu package pages"""
        # This is a simplified implementation - in practice, you might want to
        # use the Ubuntu API or parse Packages.gz files
        logger.debug(f"Web lookup for package {package_name} not implemented yet")
        return None
    
    def resolve_dependencies(self, packages: List[str], blacklist: Set[str] = None) -> Set[str]:
        """Resolve package dependencies recursively"""
        if blacklist is None:
            blacklist = self.default_blacklist.copy()
            
        resolved = set()
        to_process = set(packages)
        processed = set()
        
        while to_process:
            package = to_process.pop()
            if package in processed or package in blacklist:
                continue
                
            processed.add(package)
            resolved.add(package)
            
            # Get dependencies for this package
            deps = self._get_package_dependencies(package)
            for dep in deps:
                if dep not in processed and dep not in blacklist:
                    to_process.add(dep)
                    
        return resolved
    
    def _get_package_dependencies(self, package_name: str) -> List[str]:
        """Get dependencies for a specific package"""
        try:
            if shutil.which('apt-cache'):
                result = subprocess.run(['apt-cache', 'depends', package_name], 
                                      capture_output=True, text=True, check=False)
                if result.returncode == 0:
                    return self._parse_dependencies(result.stdout)
        except Exception as e:
            logger.debug(f"Could not get dependencies for {package_name}: {e}")
            
        return []
    
    def _parse_dependencies(self, depends_output: str) -> List[str]:
        """Parse apt-cache depends output"""
        deps = []
        for line in depends_output.split('\n'):
            line = line.strip()
            if line.startswith('Depends:'):
                dep = line.split(':', 1)[1].strip()
                # Remove version constraints and alternatives
                dep = re.sub(r'\\s*\\([^)]*\\)', '', dep)
                dep = dep.split('|')[0].strip()  # Take first alternative
                if dep and not dep.startswith('<') and not dep.startswith('|'):
                    deps.append(dep)
        return deps
    
    def download_package(self, package_name: str) -> Optional[Path]:
        """Download a package .deb file"""
        # Check if already cached
        cached_files = list(self.packages_cache_dir.glob(f"{package_name}_*.deb"))
        if cached_files:
            logger.debug(f"Using cached package: {cached_files[0]}")
            return cached_files[0]
            
        # Find package URL
        package_url = self._find_package_url(package_name)
        if not package_url:
            logger.error(f"Could not find download URL for package {package_name}")
            return None
            
        # Download the package
        try:
            filename = package_url.split('/')[-1]
            local_path = self.packages_cache_dir / filename
            
            logger.info(f"Downloading {package_name} from {package_url}")
            urllib.request.urlretrieve(package_url, local_path)
            
            return local_path
            
        except Exception as e:
            logger.error(f"Failed to download {package_name}: {e}")
            return None
    
    def _find_package_url(self, package_name: str) -> Optional[str]:
        """Find the download URL for a package"""
        # Try different repository sections
        sections = ['main', 'universe', 'restricted', 'multiverse']
        
        for section in sections:
            # Get the first letter for directory structure
            first_letter = package_name[0]
            if package_name.startswith('lib'):
                first_letter = package_name[:4]  # lib* packages are in lib/ subdirectory
            
            # Construct potential URL
            url_path = f"pool/{section}/{first_letter}/{package_name}/"
            full_url = f"{self.repo_base}/{url_path}"
            
            try:
                # Try to list directory contents
                response = urllib.request.urlopen(full_url)
                content = response.read().decode('utf-8')
                
                # Look for .deb files matching our architecture
                pattern = rf'href="([^"]*{package_name}[^"]*_{self.arch}\\.deb)"'
                matches = re.findall(pattern, content)
                
                if matches:
                    # Return the most recent version (last in list)
                    return full_url + matches[-1]
                    
            except Exception as e:
                logger.debug(f"Could not access {full_url}: {e}")
                continue
                
        return None
    
    def extract_package(self, deb_path: Path, extract_dir: Path) -> bool:
        """Extract a .deb package to a directory"""
        try:
            extract_dir.mkdir(parents=True, exist_ok=True)
            
            # Use dpkg-deb to extract if available, otherwise use ar + tar
            if shutil.which('dpkg-deb'):
                result = subprocess.run(['dpkg-deb', '-x', str(deb_path), str(extract_dir)], 
                                      check=True, capture_output=True)
            else:
                # Fallback extraction method
                self._extract_deb_manual(deb_path, extract_dir)
                
            logger.debug(f"Extracted {deb_path.name} to {extract_dir}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to extract {deb_path}: {e}")
            return False
    
    def _extract_deb_manual(self, deb_path: Path, extract_dir: Path):
        """Manual .deb extraction using ar and tar"""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            
            # Extract .deb with ar
            subprocess.run(['ar', 'x', str(deb_path)], cwd=temp_path, check=True)
            
            # Find and extract data.tar.*
            data_files = list(temp_path.glob('data.tar.*'))
            if not data_files:
                raise UbuntuPackageError(f"No data.tar.* found in {deb_path}")
                
            data_file = data_files[0]
            
            # Extract data archive
            if data_file.suffix == '.xz':
                subprocess.run(['tar', '-xJf', str(data_file), '-C', str(extract_dir)], check=True)
            elif data_file.suffix == '.gz':
                subprocess.run(['tar', '-xzf', str(data_file), '-C', str(extract_dir)], check=True)
            else:
                subprocess.run(['tar', '-xf', str(data_file), '-C', str(extract_dir)], check=True)
    
    def transform_directory(self, directory: Path, transformations: Dict) -> None:
        """Apply transformations to the extracted directory"""
        if 'remove_files' in transformations:
            self._remove_files(directory, transformations['remove_files'])
            
        if 'remove_directories' in transformations:
            self._remove_directories(directory, transformations['remove_directories'])
            
        if 'move_files' in transformations:
            self._move_files(directory, transformations['move_files'])
            
        if 'create_symlinks' in transformations:
            self._create_symlinks(directory, transformations['create_symlinks'])
    
    def _remove_files(self, directory: Path, patterns: List[str]) -> None:
        """Remove files matching patterns"""
        for pattern in patterns:
            for file_path in directory.rglob(pattern):
                if file_path.is_file():
                    logger.debug(f"Removing file: {file_path}")
                    file_path.unlink()
    
    def _remove_directories(self, directory: Path, patterns: List[str]) -> None:
        """Remove directories matching patterns"""
        for pattern in patterns:
            for dir_path in directory.rglob(pattern):
                if dir_path.is_dir():
                    logger.debug(f"Removing directory: {dir_path}")
                    shutil.rmtree(dir_path)
    
    def _move_files(self, directory: Path, moves: Dict[str, str]) -> None:
        """Move files from source to destination patterns"""
        for src_pattern, dst_pattern in moves.items():
            for src_path in directory.rglob(src_pattern):
                if src_path.is_file():
                    dst_path = directory / dst_pattern
                    dst_path.parent.mkdir(parents=True, exist_ok=True)
                    logger.debug(f"Moving {src_path} to {dst_path}")
                    shutil.move(str(src_path), str(dst_path))
    
    def _create_symlinks(self, directory: Path, symlinks: Dict[str, str]) -> None:
        """Create symbolic links"""
        for link_path, target in symlinks.items():
            full_link_path = directory / link_path
            full_link_path.parent.mkdir(parents=True, exist_ok=True)
            if full_link_path.exists():
                full_link_path.unlink()
            logger.debug(f"Creating symlink: {link_path} -> {target}")
            full_link_path.symlink_to(target)
    
    def generate_manifest(self, directory: Path, manifest_path: Path, 
                         prefix: str = '/usr') -> None:
        """Generate OSv manifest file for the directory contents"""
        with open(manifest_path, 'w') as manifest:
            for file_path in directory.rglob('*'):
                if file_path.is_file():
                    # Calculate relative path from directory
                    rel_path = file_path.relative_to(directory)
                    
                    # Skip certain directories that shouldn't be in the manifest
                    if any(part.startswith('.') for part in rel_path.parts):
                        continue
                        
                    # Create OSv path
                    if str(rel_path).startswith('usr/'):
                        osv_path = '/' + str(rel_path)
                    else:
                        osv_path = prefix + '/' + str(rel_path)
                    
                    manifest.write(f"{osv_path}: {file_path}\\n")
    
    def process_packages(self, packages: List[str], output_dir: Path, 
                        resolve_deps: bool = True, blacklist: Set[str] = None,
                        transformations: Dict = None, manifest_prefix: str = '/usr') -> bool:
        """Main method to process a list of packages"""
        try:
            # Resolve dependencies if requested
            if resolve_deps:
                logger.info("Resolving dependencies...")
                all_packages = self.resolve_dependencies(packages, blacklist)
                logger.info(f"Resolved {len(packages)} packages to {len(all_packages)} total packages")
            else:
                all_packages = set(packages)
            
            # Create output directory
            output_dir.mkdir(parents=True, exist_ok=True)
            extract_dir = output_dir / 'extracted'
            extract_dir.mkdir(exist_ok=True)
            
            # Download and extract packages
            failed_packages = []
            for package in sorted(all_packages):
                logger.info(f"Processing package: {package}")
                
                # Download package
                deb_path = self.download_package(package)
                if not deb_path:
                    failed_packages.append(package)
                    continue
                
                # Extract package
                if not self.extract_package(deb_path, extract_dir):
                    failed_packages.append(package)
                    continue
            
            if failed_packages:
                logger.warning(f"Failed to process packages: {', '.join(failed_packages)}")
            
            # Apply transformations
            if transformations:
                logger.info("Applying transformations...")
                self.transform_directory(extract_dir, transformations)
            
            # Generate manifest
            manifest_path = output_dir / 'usr.manifest'
            logger.info(f"Generating manifest: {manifest_path}")
            self.generate_manifest(extract_dir, manifest_path, manifest_prefix)
            
            return len(failed_packages) == 0
            
        except Exception as e:
            logger.error(f"Error processing packages: {e}")
            return False


def load_config_file(config_path: Path) -> Dict:
    """Load configuration from JSON file"""
    try:
        with open(config_path, 'r') as f:
            return json.load(f)
    except Exception as e:
        logger.error(f"Failed to load config file {config_path}: {e}")
        return {}


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Download and process Ubuntu packages for OSv',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Download curl package with dependencies
  ubuntu_packages.py --packages curl --output-dir /tmp/curl_output

  # Download multiple packages without dependencies
  ubuntu_packages.py --packages "curl,wget,vim" --no-deps --output-dir /tmp/tools

  # Use configuration file
  ubuntu_packages.py --config myapp.json --output-dir /tmp/myapp

  # Specify architecture and Ubuntu release
  ubuntu_packages.py --packages nginx --arch aarch64 --ubuntu-release focal --output-dir /tmp/nginx

Configuration file format (JSON):
{
    "packages": ["curl", "libcurl4"],
    "blacklist": ["systemd", "bash"],
    "transformations": {
        "remove_files": ["*.a", "*.la"],
        "remove_directories": ["usr/share/doc", "usr/share/man"]
    },
    "manifest_prefix": "/usr",
    "cmdline": "/usr/bin/curl --help"
}
        """)
    
    parser.add_argument('--packages', '-p', 
                       help='Comma-separated list of Ubuntu packages to download')
    parser.add_argument('--config', '-c', type=Path,
                       help='JSON configuration file')
    parser.add_argument('--output-dir', '-o', type=Path, required=True,
                       help='Output directory for extracted packages')
    parser.add_argument('--arch', '-a', default='amd64',
                       choices=['amd64', 'arm64', 'armhf', 'i386'],
                       help='Target architecture (default: amd64)')
    parser.add_argument('--ubuntu-release', '-r', default='focal',
                       help='Ubuntu release codename (default: focal)')
    parser.add_argument('--cache-dir', type=Path,
                       help='Cache directory for downloaded packages')
    parser.add_argument('--no-deps', action='store_true',
                       help='Do not resolve dependencies')
    parser.add_argument('--blacklist', 
                       help='Comma-separated list of packages to blacklist')
    parser.add_argument('--manifest-prefix', default='/usr',
                       help='Prefix for manifest paths (default: /usr)')
    parser.add_argument('--cmdline',
                       help='Command line to write to cmdline file')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Enable verbose output')
    
    args = parser.parse_args()
    
    # Load configuration
    config = {}
    if args.config:
        config = load_config_file(args.config)
    
    # Get packages list
    packages = []
    if args.packages:
        packages.extend(args.packages.split(','))
    if 'packages' in config:
        packages.extend(config['packages'])
    
    if not packages:
        logger.error("No packages specified. Use --packages or --config.")
        return 1
    
    # Set up blacklist
    blacklist = set()
    if args.blacklist:
        blacklist.update(args.blacklist.split(','))
    if 'blacklist' in config:
        blacklist.update(config['blacklist'])
    
    # Get transformations
    transformations = config.get('transformations', {})
    
    # Create handler
    handler = UbuntuPackageHandler(
        arch=args.arch,
        ubuntu_release=args.ubuntu_release,
        cache_dir=args.cache_dir,
        verbose=args.verbose
    )
    
    # Process packages
    success = handler.process_packages(
        packages=packages,
        output_dir=args.output_dir,
        resolve_deps=not args.no_deps,
        blacklist=blacklist if blacklist else None,
        transformations=transformations,
        manifest_prefix=args.manifest_prefix or config.get('manifest_prefix', '/usr')
    )
    
    # Write command line file if specified
    cmdline = args.cmdline or config.get('cmdline')
    if cmdline:
        cmdline_path = args.output_dir / 'cmdline'
        with open(cmdline_path, 'w') as f:
            f.write(cmdline)
        logger.info(f"Wrote command line to {cmdline_path}")
    
    if success:
        logger.info("Package processing completed successfully")
        return 0
    else:
        logger.error("Package processing completed with errors")
        return 1


if __name__ == '__main__':
    sys.exit(main())