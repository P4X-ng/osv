include ../common.gmk

module_out := $(out)/modules/java-base
export module_out

include common.gmk
include $(out)/gen/config/kernel_conf.mk

jar-target := $(java-base-path)/runjava-non-isolated/target/runjava-non-isolated.jar

ifeq ($(conf_memory_jvm_balloon),1)
java-targets := $(module_out)/java.so $(module_out)/jni/monitor.so $(module_out)/jvm/jni_helpers.o $(module_out)/jvm/java_api.o $(module_out)/balloon/jvm_balloon.o
else
java-targets := $(module_out)/java.so $(module_out)/jni/monitor.so $(module_out)/jvm/jni_helpers.o $(module_out)/jvm/java_api.o
endif

module: all

all: $(init) $(java-targets) $(jar-target)

init:
	@echo "  MKDIRS"
	$(call very-quiet, mkdir -p $(module_out)/jni)
	$(call very-quiet, mkdir -p $(module_out)/jvm)
	$(call very-quiet, mkdir -p $(module_out)/balloon)
.PHONY: init

$(module_out)/java.o: $(java-base-path)/java.cc | init
	$(call quiet, $(CXX) $(CXXFLAGS) -o $@ -c $(java-base-path)/java.cc -MMD, CXX $@)

java-objects := $(module_out)/java.o $(module_out)/jvm/java_api.o $(module_out)/jvm/jni_helpers.o
ifeq ($(conf_memory_jvm_balloon),1)
java-objects += $(module_out)/balloon/jvm_balloon.o
endif

$(module_out)/java.so: $(java-objects)
	$(call quiet, $(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^, LINK $@)

comma := ,

$(jar-target): $(wildcard $(java-base-path)/runjava-non-isolated/src/main/java/io/osv/nonisolated/*.java) \
		$(wildcard $(java-base-path)/runjava-common/src/main/java/io/osv/*.java) \
		$(wildcard $(java-base-path)/runjava-common/src/main/java/io/osv/util/*.java) \
		$(java-base-path)/pom.xml $(java-base-path)/runjava-common/pom.xml \
		$(java-base-path)/runjava-non-isolated/pom.xml
	$(call quiet, cd $(java-base-path) && JAVA_HOME=$(java_jdk_path) mvn -q --projects :runjava-common$(comma):runjava-non-isolated package -DskipTests=true, MVN $@)

clean:
	cd $(java-base-path) && mvn -q clean
	-rm -f dependency-reduced-pom.xml
	$(call very-quiet, $(RM) -rf $(module_out))
