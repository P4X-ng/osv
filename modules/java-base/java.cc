/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <osv/mempool.hh>
#include <osv/app.hh>

// Minimal Java launcher that calculates -Xmx and launches native /usr/bin/java
// This replaces the complex JNI-based wrapper while preserving essential functionality

inline bool starts_with(const char *s, const char *prefix)
{
    return !strncmp(s, prefix, strlen(prefix));
}

static bool has_heap_option(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (starts_with(argv[i], "-Xmx") || starts_with(argv[i], "-mx")) {
            return true;
        }
    }
    return false;
}

static std::string calculate_heap_size()
{
    // Calculate a reasonable heap size based on available memory
    // Use a conservative approach: 75% of available memory, minimum 64MB
    size_t available_mb = memory::stats::free() >> 20;  // Convert to MB
    size_t heap_mb = std::max(64UL, (available_mb * 3) / 4);
    
    return "-Xmx" + std::to_string(heap_mb) + "M";
}

static int java_main(int argc, char **argv)
{
    std::cout << "java.so: Launching native Java with OSv integration\n";
    
    // Build arguments for native java
    std::vector<std::string> java_args;
    java_args.push_back("/usr/bin/java");
    
    // Add heap size if not specified by user
    if (!has_heap_option(argc, argv)) {
        java_args.push_back(calculate_heap_size());
        std::cout << "java.so: Auto-calculated heap size: " << java_args.back() << "\n";
    }
    
    // Add OSvGCMonitor to classpath for balloon functionality
    java_args.push_back("-cp");
    java_args.push_back("/java/osv-gc-monitor.jar");
    
    // Add all original arguments (skip argv[0] which is our program name)
    for (int i = 1; i < argc; i++) {
        java_args.push_back(argv[i]);
    }
    
    // Convert to char* array for execv
    std::vector<char*> exec_args;
    for (auto& arg : java_args) {
        exec_args.push_back(const_cast<char*>(arg.c_str()));
    }
    exec_args.push_back(nullptr);
    
    // Launch native java
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - exec native java
        execv("/usr/bin/java", exec_args.data());
        // If we get here, exec failed
        std::cerr << "java.so: Failed to exec /usr/bin/java: " << strerror(errno) << "\n";
        exit(1);
    } else if (pid > 0) {
        // Parent process - wait for child
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    } else {
        std::cerr << "java.so: Failed to fork: " << strerror(errno) << "\n";
        return 1;
    }
}

extern "C"
int main(int argc, char **argv)
{
    return java_main(argc, argv);
}
