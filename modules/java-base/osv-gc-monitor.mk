# OSv GC Monitor - Standalone JAR for balloon functionality
# This contains only the essential OSvGCMonitor.java class needed for JVM balloon

SRC_DIR = osv-gc-monitor/src/main/java
JAR_FILE = osv-gc-monitor.jar

.PHONY: all clean

all: $(JAR_FILE)

$(JAR_FILE): $(SRC_DIR)/io/osv/OSvGCMonitor.java
	@echo "Building OSv GC Monitor JAR..."
	@mkdir -p build/classes
	@javac -d build/classes -cp . $(SRC_DIR)/io/osv/OSvGCMonitor.java
	@jar cf $(JAR_FILE) -C build/classes .
	@echo "Created $(JAR_FILE)"

clean:
	@rm -rf build $(JAR_FILE)