include ../common.gmk

# GDB stub module
module: libgdb-stub.so

CXXFLAGS = -g -rdynamic -Wall -std=c++11 -fPIC $(INCLUDES) -O2
LDFLAGS = -shared

# Source files
SOURCES = gdb_stub.cc gdb_protocol.cc gdb_transport.cc gdb_arch.cc main.cc

# Object files
OBJECTS = $(SOURCES:.cc=.o)

libgdb-stub.so: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o libgdb-stub.so

.PHONY: module clean