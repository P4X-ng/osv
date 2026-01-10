include ../common.gmk

module_out := $(out)/modules/gdb-stub

INCLUDES += -I. -I$(src)/include

# Compiler flags
CXXFLAGS  = -g -Wall -std=$(conf_cxx_level) -fPIC $(COMMON) -O2

# Target
TARGET = gdb-stub

# Source files
CPP_FILES := gdb-stub.cc main.cc
OBJ_FILES := $(addprefix $(module_out)/,$(CPP_FILES:.cc=.o))

# Dependencies
DEPS := $(OBJ_FILES:.o=.d)

# Libraries
DYN_LIBS = -lpthread

quiet = $(if $V, $1, @echo " $2"; $1)
very-quiet = $(if $V, $1, @$1)

module: all

all: $(module_out)/lib$(TARGET).so
	$(call very-quiet, $(src)/scripts/manifest_from_host.sh $(module_out)/lib$(TARGET).so > usr.manifest)

$(module_out)/lib$(TARGET).so: $(OBJ_FILES)
	$(call quiet, $(CXX) $(CXXFLAGS) -shared -o $@ $^ $(DYN_LIBS), LINK $@)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(module_out)/%.o: %.cc
	$(call very-quiet, mkdir -p $(module_out))
	$(call quiet, $(CXX) $(CXXFLAGS) -c -MMD -o $@ $<, CXX $@)

clean:
	$(call quiet, $(RM) -f $(TARGET), CLEAN)
	$(call very-quiet, $(RM) -rf $(module_out))
	$(call very-quiet, $(RM) -f usr.manifest)

.PHONY: module all clean
