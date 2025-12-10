# Example module using Ubuntu packages for curl
# This demonstrates the new Ubuntu package-based module system

SRC = $(shell readlink -f $(dir $(lastword $(MAKEFILE_LIST))))
OSV_ROOT = $(SRC)/../..
UBUNTU_PACKAGES_SCRIPT = $(OSV_ROOT)/scripts/ubuntu_packages.py

# Configuration
PACKAGES = curl,libcurl4
CMDLINE = /usr/bin/curl --help
ARCH ?= amd64
UBUNTU_RELEASE ?= focal

# Output directory for this module
MODULE_DIR = $(OSV_BUILD_PATH)/modules/curl-ubuntu

.PHONY: module clean

module: $(MODULE_DIR)/usr.manifest

$(MODULE_DIR)/usr.manifest: $(SRC)/curl-ubuntu.json
	@echo "Building curl-ubuntu module using Ubuntu packages..."
	@mkdir -p $(MODULE_DIR)
	@$(UBUNTU_PACKAGES_SCRIPT) \
		--config $(SRC)/curl-ubuntu.json \
		--output-dir $(MODULE_DIR) \
		--arch $(ARCH) \
		--ubuntu-release $(UBUNTU_RELEASE) \
		--cmdline "$(CMDLINE)"
	@echo "$(MODULE_DIR)/usr.manifest: $(MODULE_DIR)/usr.manifest" > $(OSV_BUILD_PATH)/usr.manifest
	@if [ -f $(MODULE_DIR)/cmdline ]; then \
		cp $(MODULE_DIR)/cmdline $(OSV_BUILD_PATH)/cmdline; \
	fi

clean:
	@rm -rf $(MODULE_DIR)