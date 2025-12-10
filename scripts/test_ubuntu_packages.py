#!/usr/bin/env python3
"""
Test script for Ubuntu package system

This script tests the basic functionality of the Ubuntu package handler.
"""

import tempfile
import shutil
import sys
from pathlib import Path

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent))

from ubuntu_packages import UbuntuPackageHandler

def test_basic_functionality():
    """Test basic package handling functionality"""
    print("Testing Ubuntu package handler...")
    
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        
        # Create handler
        handler = UbuntuPackageHandler(
            arch='amd64',
            ubuntu_release='focal',
            cache_dir=temp_path / 'cache',
            verbose=True
        )
        
        # Test package info (this might not work without apt-cache)
        print("Testing package info retrieval...")
        info = handler.get_package_info('curl')
        if info:
            print(f"Found package info for curl: {info.get('Package', 'Unknown')}")
        else:
            print("Package info retrieval not available (apt-cache not found)")
        
        # Test dependency resolution
        print("Testing dependency resolution...")
        deps = handler.resolve_dependencies(['curl'], blacklist={'systemd', 'bash'})
        print(f"Resolved dependencies for curl: {sorted(deps)}")
        
        # Test URL finding (this will likely fail without network)
        print("Testing package URL finding...")
        url = handler._find_package_url('curl')
        if url:
            print(f"Found URL for curl: {url}")
        else:
            print("Could not find URL for curl (network/repository issue)")
        
        print("Basic functionality test completed.")
        return True

def test_config_loading():
    """Test configuration file loading"""
    print("Testing configuration loading...")
    
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        
        # Create test config
        config_path = temp_path / 'test.json'
        config_content = '''
{
    "packages": ["curl", "wget"],
    "blacklist": ["systemd"],
    "transformations": {
        "remove_files": ["*.a"]
    },
    "cmdline": "/usr/bin/curl --help"
}
'''
        with open(config_path, 'w') as f:
            f.write(config_content)
        
        # Load config
        from ubuntu_packages import load_config_file
        config = load_config_file(config_path)
        
        assert config['packages'] == ['curl', 'wget']
        assert config['blacklist'] == ['systemd']
        assert config['cmdline'] == '/usr/bin/curl --help'
        
        print("Configuration loading test passed.")
        return True

def main():
    """Run all tests"""
    print("Running Ubuntu package system tests...")
    
    try:
        test_basic_functionality()
        test_config_loading()
        print("All tests completed successfully!")
        return 0
    except Exception as e:
        print(f"Test failed: {e}")
        return 1

if __name__ == '__main__':
    sys.exit(main())