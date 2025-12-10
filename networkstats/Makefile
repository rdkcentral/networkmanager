# Makefile for ThunderJsonRPCProvider Test Application
# Copyright 2025 RDK Management

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wno-deprecated-declarations -g -DTEST_MAIN

# Include directories
INCLUDES := -I. \
            -I/usr/include \
            -I/usr/local/include \
            -I/opt/homebrew/include

# Library directories
LIBDIRS := -L/usr/lib \
           -L/usr/lib/x86_64-linux-gnu \
           -L/usr/local/lib \
           -L/opt/homebrew/lib

# Libraries to link
LIBS := -ljsonrpccpp-client \
        -ljsonrpccpp-common \
        -ljsoncpp

# Source files
SOURCES := ThunderJsonRPCProvider.cpp

# Output binary
TARGET := thunder-jsonrpc-provider-test

# Build rules
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LIBDIRS) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	mkdir -p ~/bin
	cp $(TARGET) ~/bin/

.PHONY: all clean install help

help:
	@echo "ThunderJsonRPCProvider Test Application Build"
	@echo "=============================================="
	@echo ""
	@echo "Prerequisites:"
	@echo "  - libjsonrpccpp-client (JSON-RPC C++ client library)"
	@echo "  - libjsonrpccpp-common (JSON-RPC C++ common library)"
	@echo "  - jsoncpp (JSON C++ library)"
	@echo ""
	@echo "Install dependencies on Ubuntu/Debian:"
	@echo "  sudo apt-get install libjsonrpccpp-dev libjsoncpp-dev"
	@echo ""
	@echo "Install dependencies on macOS:"
	@echo "  brew install jsoncpp libjson-rpc-cpp"
	@echo ""
	@echo "Build commands:"
	@echo "  make         - Build the test application"
	@echo "  make clean   - Remove built files"
	@echo "  make install - Install to ~/bin"
	@echo "  make help    - Show this help message"
	@echo ""
