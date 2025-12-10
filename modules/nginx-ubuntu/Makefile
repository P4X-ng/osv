# Example module using Ubuntu packages for nginx web server
# This demonstrates a more complex Ubuntu package-based module

SRC = $(shell readlink -f $(dir $(lastword $(MAKEFILE_LIST))))
OSV_ROOT = $(SRC)/../..
UBUNTU_PACKAGES_SCRIPT = $(OSV_ROOT)/scripts/ubuntu_packages.py

# Configuration
ARCH ?= amd64
UBUNTU_RELEASE ?= focal

# Output directory for this module
MODULE_DIR = $(OSV_BUILD_PATH)/modules/nginx-ubuntu

.PHONY: module clean

module: $(MODULE_DIR)/usr.manifest

$(MODULE_DIR)/usr.manifest: $(SRC)/nginx-ubuntu.json
	@echo "Building nginx-ubuntu module using Ubuntu packages..."
	@mkdir -p $(MODULE_DIR)
	@$(UBUNTU_PACKAGES_SCRIPT) \
		--config $(SRC)/nginx-ubuntu.json \
		--output-dir $(MODULE_DIR) \
		--arch $(ARCH) \
		--ubuntu-release $(UBUNTU_RELEASE)
	@echo "$(MODULE_DIR)/usr.manifest: $(MODULE_DIR)/usr.manifest" > $(OSV_BUILD_PATH)/usr.manifest
	@if [ -f $(MODULE_DIR)/cmdline ]; then \
		cp $(MODULE_DIR)/cmdline $(OSV_BUILD_PATH)/cmdline; \
	fi

clean:
	@rm -rf $(MODULE_DIR)