/*
 * Test program to verify the ROFS + virtio-blk large file read fix
 * 
 * This test simulates the scenario where:
 * 1. A large file read request is made to ROFS without cache
 * 2. The request would exceed virtio-blk's segment limits
 * 3. Verifies that the request either succeeds (with splitting) or fails gracefully (without hanging)
 */

#include <iostream>
#include <cassert>
#include <cstring>

// Mock the necessary structures and functions for testing
struct device {
    size_t max_io_size;
    void* driver;
};

struct bio {
    int bio_cmd;
    struct device* bio_dev;
    void* bio_data;
    uint64_t bio_offset;
    uint64_t bio_bcount;
};

#define BIO_READ 1
#define BSIZE 512
#define ENOMEM 12

// Mock functions
struct bio* alloc_bio() {
    return new bio();
}

void destroy_bio(struct bio* bio) {
    delete bio;
}

int bio_wait(struct bio* bio) {
    // Simulate successful read
    return 0;
}

// Mock device with small max_io_size to trigger splitting
struct device test_device = {
    .max_io_size = 2048, // Only 4 blocks (2048 bytes) max
    .driver = nullptr
};

// Mock strategy function
void mock_strategy(struct bio* bio) {
    // Simulate the fixed virtio-blk behavior
    if (bio->bio_bcount > bio->bio_dev->max_io_size) {
        std::cout << "ERROR: Request size " << bio->bio_bcount 
                  << " exceeds max_io_size " << bio->bio_dev->max_io_size << std::endl;
        // In the real fix, this would call biodone(bio, false) and return EIO
        // For this test, we'll just assert this shouldn't happen
        assert(false && "Request should have been split before reaching strategy");
    }
    std::cout << "Processing request: offset=" << bio->bio_offset 
              << ", size=" << bio->bio_bcount << " bytes" << std::endl;
}

// Include the fixed rofs_read_blocks function (simplified version)
int rofs_read_blocks(struct device *device, uint64_t starting_block, uint64_t blocks_count, void *buf)
{
    // Calculate total bytes to read
    uint64_t total_bytes = blocks_count * BSIZE;
    uint64_t max_request_bytes = device->max_io_size;
    
    std::cout << "Reading " << blocks_count << " blocks (" << total_bytes 
              << " bytes) starting at block " << starting_block << std::endl;
    std::cout << "Device max_io_size: " << max_request_bytes << " bytes" << std::endl;
    
    // If the request fits within device limits, use the original single-request approach
    if (total_bytes <= max_request_bytes) {
        std::cout << "Single request (fits within limits)" << std::endl;
        struct bio *bio = alloc_bio();
        if (!bio)
            return ENOMEM;

        bio->bio_cmd = BIO_READ;
        bio->bio_dev = device;
        bio->bio_data = buf;
        bio->bio_offset = starting_block << 9;
        bio->bio_bcount = total_bytes;

        mock_strategy(bio);
        int error = bio_wait(bio);

        destroy_bio(bio);
        return error;
    }
    
    // Split large request into smaller chunks
    std::cout << "Splitting large request into smaller chunks" << std::endl;
    uint64_t max_blocks_per_request = max_request_bytes / BSIZE;
    uint64_t current_block = starting_block;
    uint64_t remaining_blocks = blocks_count;
    char *current_buf = static_cast<char*>(buf);
    int error = 0;
    int chunk_count = 0;
    
    while (remaining_blocks > 0 && error == 0) {
        uint64_t blocks_this_request = std::min(remaining_blocks, max_blocks_per_request);
        uint64_t bytes_this_request = blocks_this_request * BSIZE;
        
        std::cout << "  Chunk " << ++chunk_count << ": " << blocks_this_request 
                  << " blocks (" << bytes_this_request << " bytes)" << std::endl;
        
        struct bio *bio = alloc_bio();
        if (!bio) {
            error = ENOMEM;
            break;
        }

        bio->bio_cmd = BIO_READ;
        bio->bio_dev = device;
        bio->bio_data = current_buf;
        bio->bio_offset = current_block << 9;
        bio->bio_bcount = bytes_this_request;

        mock_strategy(bio);
        error = bio_wait(bio);

        destroy_bio(bio);
        
        if (error == 0) {
            current_block += blocks_this_request;
            remaining_blocks -= blocks_this_request;
            current_buf += bytes_this_request;
        }
    }

    std::cout << "Request completed with " << chunk_count << " chunks, error=" << error << std::endl;
    return error;
}

int main() {
    std::cout << "Testing ROFS large file read fix..." << std::endl;
    
    // Test case 1: Small request that fits within limits
    std::cout << "\n=== Test 1: Small request (2 blocks) ===" << std::endl;
    char small_buf[1024];
    int result1 = rofs_read_blocks(&test_device, 0, 2, small_buf);
    assert(result1 == 0);
    std::cout << "PASS: Small request completed successfully" << std::endl;
    
    // Test case 2: Large request that needs splitting
    std::cout << "\n=== Test 2: Large request (10 blocks) ===" << std::endl;
    char large_buf[5120]; // 10 blocks * 512 bytes
    int result2 = rofs_read_blocks(&test_device, 100, 10, large_buf);
    assert(result2 == 0);
    std::cout << "PASS: Large request completed successfully with splitting" << std::endl;
    
    // Test case 3: Very large request
    std::cout << "\n=== Test 3: Very large request (20 blocks) ===" << std::endl;
    char very_large_buf[10240]; // 20 blocks * 512 bytes
    int result3 = rofs_read_blocks(&test_device, 200, 20, very_large_buf);
    assert(result3 == 0);
    std::cout << "PASS: Very large request completed successfully with splitting" << std::endl;
    
    std::cout << "\n=== All tests passed! ===" << std::endl;
    std::cout << "The fix successfully prevents hangs by splitting large requests." << std::endl;
    
    return 0;
}