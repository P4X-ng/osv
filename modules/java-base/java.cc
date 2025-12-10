/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

/*
 * Minimal Java launcher for OSv
 * 
 * This is a minimal wrapper around the native Java launcher that:
 * 1. Optionally calculates and sets -Xmx based on available memory
 * 2. Passes all arguments to the native java binary via execve
 * 
 * This replaces the previous complex JNI-based wrapper that had to be
 * updated for every Java version. The native java binary is now executed
 * directly, inheriting all its features and compatibility.
 */

#include <unistd.h>
#include <errno.h>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include <osv/mempool.hh>

// Possible locations for the native java binary
#ifdef __aarch64__
#define JAVA_BIN_PATH1 "/usr/lib/jvm/bin/java"
#define JAVA_BIN_PATH2 "/usr/lib/jvm/jre/bin/java"
#define JAVA_BIN_PATH3 "/usr/lib/jvm/java/bin/java"
#else
#define JAVA_BIN_PATH1 "/usr/lib/jvm/bin/java"
#define JAVA_BIN_PATH2 "/usr/lib/jvm/jre/bin/java"
#define JAVA_BIN_PATH3 "/usr/lib/jvm/java/bin/java"
#endif

static bool file_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

static const char* find_java_binary() {
    if (file_exists(JAVA_BIN_PATH1)) return JAVA_BIN_PATH1;
    if (file_exists(JAVA_BIN_PATH2)) return JAVA_BIN_PATH2;
    if (file_exists(JAVA_BIN_PATH3)) return JAVA_BIN_PATH3;
    return nullptr;
}

static bool has_heap_option(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-Xmx", 4) == 0 || 
            strncmp(argv[i], "-Xms", 4) == 0 ||
            strncmp(argv[i], "-mx", 3) == 0 ||
            strncmp(argv[i], "-ms", 3) == 0) {
            return true;
        }
    }
    return false;
}

static std::string calculate_xmx() {
    // Calculate a reasonable heap size based on available memory
    // Use a conservative approach: use up to 75% of free memory
    size_t free_mem = memory::stats::free();
    size_t xmx_bytes = (free_mem * 3) / 4;
    
    // Minimum heap: 32MB
    if (xmx_bytes < 32 * 1024 * 1024) {
        xmx_bytes = 32 * 1024 * 1024;
    }
    
    // Convert to MB for readability
    size_t xmx_mb = xmx_bytes / (1024 * 1024);
    
    std::string xmx_arg = "-Xmx" + std::to_string(xmx_mb) + "M";
    std::cout << "java.so: Auto-calculated heap size: " << xmx_arg << std::endl;
    return xmx_arg;
}

static int java_main(int argc, char** argv) {
    // Find the native java binary
    const char* java_path = find_java_binary();
    if (!java_path) {
        std::cerr << "java.so: ERROR: Could not find native java binary at expected locations\n";
        std::cerr << "  Searched: " << JAVA_BIN_PATH1 << ", " << JAVA_BIN_PATH2 << ", " << JAVA_BIN_PATH3 << "\n";
        return 1;
    }
    
    std::cout << "java.so: Using native Java launcher at: " << java_path << std::endl;
    
    // Build the argument list for execve
    std::vector<std::string> args_storage;
    std::vector<char*> exec_args;
    exec_args.push_back(const_cast<char*>(java_path));
    
    // Optionally add -Xmx if not already specified
    bool should_add_xmx = !has_heap_option(argc, argv);
    if (should_add_xmx) {
        args_storage.push_back(calculate_xmx());
        exec_args.push_back(const_cast<char*>(args_storage.back().c_str()));
    }
    
    // Pass through all arguments from argv[1] onwards
    for (int i = 1; i < argc; i++) {
        exec_args.push_back(argv[i]);
    }
    exec_args.push_back(nullptr);
    
    // Print the command we're about to execute (for debugging)
    std::cout << "java.so: Executing:";
    for (size_t i = 0; i < exec_args.size() - 1; i++) {
        std::cout << " " << exec_args[i];
    }
    std::cout << std::endl;
    
    // Execute the native java binary, replacing the current process
    // This is the simplest and most efficient approach
    execve(java_path, exec_args.data(), environ);
    
    // If we get here, execve failed
    std::cerr << "java.so: ERROR: Failed to execute " << java_path << ": " << strerror(errno) << "\n";
    return 1;
}

extern "C"
int main(int argc, char** argv) {
    // Simply execute java_main which will replace this process with native java
    // No need for cleanup code since execve replaces the entire process
    return java_main(argc, argv);
}
