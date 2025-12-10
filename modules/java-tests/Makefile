include ../common.gmk

module_out := $(out)/modules/java-tests
export module_out

include ../java-base/common.gmk
include $(out)/gen/config/kernel_conf.mk

javac_version := $(shell javac -version 2>&1 | grep -oP "javac \d+\.\d+")

ifeq ($(javac_version),javac 1.8)
tests-jar-target := tests/target/runjava-tests.jar

tests-jre-extension-target := tests-jre-extension/target/tests-jre-extension.jar

comma := ,

$(tests-jre-extension-target): $(wildcard tests-jre-extension/src/main/java/tests/*.java) \
                tests-jre-extension/pom.xml pom.xml
        $(call quiet, cd $(src)/modules/java-tests && JAVA_HOME=$(java_jdk_path) mvn -q --projects :tests-jre-extension package -DskipTests=true, MVN $@)

$(tests-jar-target): $(tests-jre-extension-target) \
                $(wildcard tests/src/main/java/io/osv/*.java) \
                $(wildcard tests/src/main/java/tests/*.java) \
                pom.xml tests/pom.xml
        $(call quiet, cd $(src)/modules/java-tests && \
        JAVA_HOME=$(java_jdk_path) mvn -q --projects :runjava-common$(comma):runjava-non-isolated$(comma):runjava-tests$(comma):tests-jre-extension package -DskipTests=true, MVN $@)

init:
        @echo "  MKDIRS"
        $(call very-quiet, mkdir -p $(module_out))

module: $(tests-jar-target) test_commands
else
tests-jar-target := tests-for-java9_1x/target/runjava-9-1x-tests.jar

$(tests-jar-target): $(wildcard $(java-base-path)/tests-for-java9_1x/src/main/java/io/osv//*.java)
        $(call quiet, cd $(src)/modules/java-tests/tests-for-java9_1x && \
        JAVA_HOME=$(java_jdk_path) mvn -q package -DskipTests=true, MVN $@)

module: $(tests-jar-target) test_commands
endif

java_non_isolated_cmd := 'java_non_isolated: /java.so -cp /tests/java/tests.jar org.junit.runner.JUnitCore io.osv.AllTestsThatTestNonIsolatedApp'
java_no_wrapper_cmd := 'java_no_wrapper: /usr/lib/jvm/java/bin/java -cp /tests/java/tests.jar org.junit.runner.JUnitCore io.osv.BasicTests !'
java_perms_cmd := 'java-perms: /usr/lib/jvm/java/bin/java -cp /tests/java/tests.jar io.osv.TestDomainPermissions !'

.PHONY: test_commands

ifeq ($(arch),$(host_arch))
test_commands:
        $(call very-quiet, rm -f test_commands)
        $(call very-quiet, if [ "$(javac_version)" = "javac 1.8" -a "$(conf_hide_symbols)" != "1" ]; then \
                echo $(java_non_isolated_cmd) >> test_commands; fi )
        $(call very-quiet, echo $(java_no_wrapper_cmd) >> test_commands)
        $(call very-quiet, echo $(java_perms_cmd) >> test_commands)
else
test_commands:
        $(call very-quiet, rm -f test_commands)
endif

clean:
        cd $(src)/modules/java-tests && mvn -q clean
        cd $(src)/modules/java-tests/tests-for-java9_1x && mvn -q clean
        -rm -f dependency-reduced-pom.xml test_commands
        $(call very-quiet, $(RM) -rf $(module_out))
