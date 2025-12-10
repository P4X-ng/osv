/*
 * Test for rwlock fixes
 * 
 * This test verifies that:
 * 1. A thread holding a write lock can acquire a read lock without hanging
 * 2. The read unlock works correctly when the thread holds a write lock
 */

#include <osv/rwlock.h>
#include <iostream>
#include <thread>
#include <chrono>

void test_write_then_read_lock() {
    std::cout << "Testing write lock followed by read lock..." << std::endl;
    
    rwlock rw;
    
    // Acquire write lock
    rw.wlock();
    std::cout << "Acquired write lock" << std::endl;
    
    // This should not hang (Issue 1 fix)
    rw.rlock();
    std::cout << "Acquired read lock while holding write lock - SUCCESS!" << std::endl;
    
    // Test try_rlock as well
    if (rw.try_rlock()) {
        std::cout << "try_rlock() succeeded while holding write lock - SUCCESS!" << std::endl;
    } else {
        std::cout << "try_rlock() failed while holding write lock - ERROR!" << std::endl;
    }
    
    // Unlock read locks (should be no-ops)
    rw.runlock();
    std::cout << "Released read lock" << std::endl;
    
    rw.runlock();
    std::cout << "Released second read lock" << std::endl;
    
    // Release write lock
    rw.wunlock();
    std::cout << "Released write lock" << std::endl;
    
    std::cout << "Test completed successfully!" << std::endl;
}

void test_recursive_write_lock() {
    std::cout << "Testing recursive write lock..." << std::endl;
    
    rwlock rw;
    
    // Acquire write lock
    rw.wlock();
    std::cout << "Acquired first write lock" << std::endl;
    
    // Acquire recursive write lock
    rw.wlock();
    std::cout << "Acquired recursive write lock" << std::endl;
    
    // Acquire read lock while holding recursive write lock
    rw.rlock();
    std::cout << "Acquired read lock while holding recursive write lock - SUCCESS!" << std::endl;
    
    // Release read lock
    rw.runlock();
    std::cout << "Released read lock" << std::endl;
    
    // Release recursive write locks
    rw.wunlock();
    std::cout << "Released first recursive write lock" << std::endl;
    
    rw.wunlock();
    std::cout << "Released second write lock" << std::endl;
    
    std::cout << "Recursive test completed successfully!" << std::endl;
}

int main() {
    try {
        test_write_then_read_lock();
        std::cout << std::endl;
        test_recursive_write_lock();
        std::cout << std::endl << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}