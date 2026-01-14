.PHONY: module clean

include ../common.gmk
ifneq ($(arch),$(host_arch))
$(error Cannot provide .NET when cross-compiling)
endif

SRC = $(shell readlink -f ../..)

module:
	@echo ".NET module does not require building"

clean:
	@echo "Nothing to clean in .NET module"
