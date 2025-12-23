include ../common.gmk

module_out := $(out)/modules/java-non-isolated
export module_out

include ../java-base/common.gmk
include $(out)/gen/config/kernel_conf.mk

java-targets := $(module_out)/java_non_isolated.so

# JAR files are no longer needed with minimal java wrapper
# The minimal wrapper simply execs the native java binary
# jar-targets := $(java-base-path)/runjava-non-isolated/target/runjava-non-isolated.jar

$(module_out)/java_non_isolated.o: $(java-base-path)/java.cc | init
	$(call quiet, $(CXX) $(CXXFLAGS) -o $@ -c $(java-base-path)/java.cc -MMD, CXX $@)

# Minimal java wrapper only needs java.o - no JNI helpers or balloon support needed
# Note: RUN_JAVA_NON_ISOLATED flag removed as isolated/non-isolated distinction
# is no longer relevant with minimal wrapper
java-objects := $(module_out)/java_non_isolated.o

$(module_out)/java_non_isolated.so: $(java-objects)
	$(call quiet, $(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^, LINK $@)

init:
	@echo "  MKDIRS"
	$(call very-quiet, mkdir -p $(module_out))

# JAR building removed - not needed with minimal wrapper
# comma := ,
# $(jar-targets): ...

module: $(java-targets)

clean:
	# Maven clean removed - no longer building JAR files
	# cd $(java-base-path) && mvn -q clean
	-rm -f dependency-reduced-pom.xml
	$(call very-quiet, $(RM) -rf $(module_out))
