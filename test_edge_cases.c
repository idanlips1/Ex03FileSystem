#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs.h"

#define TEST_DISK "test_disk.img"
#define MAX_TEST_FILES 50
#define LARGE_FILE_SIZE 45000  // Just under 48KB limit

// Helper function to create a fresh disk
void setup_disk() {
    if (access(TEST_DISK, F_OK) == 0) {
        remove(TEST_DISK);
    }
    if (fs_format(TEST_DISK) != 0) {
        fprintf(stderr, "Failed to format disk\n");
        exit(1);
    }
    if (fs_mount(TEST_DISK) != 0) {
        fprintf(stderr, "Failed to mount disk\n");
        exit(1);
    }
}

// Test 1: Empty file operations
void test_empty_files() {
    printf("=== Test 1: Empty File Operations ===\n");
    
    // Create empty file
    if (fs_create("empty.txt") != 0) {
        printf("FAILED: Could not create empty file\n");
        return;
    }
    
    // Read from empty file
    char buffer[100];
    int bytes_read = fs_read("empty.txt", buffer, sizeof(buffer));
    if (bytes_read != 0) {
        printf("FAILED: Empty file should return 0 bytes, got %d\n", bytes_read);
        return;
    }
    
    // Write to empty file
    char data[] = "Hello";
    if (fs_write("empty.txt", data, strlen(data)) != 0) {
        printf("FAILED: Could not write to empty file\n");
        return;
    }
    
    // Read back
    memset(buffer, 0, sizeof(buffer));
    bytes_read = fs_read("empty.txt", buffer, sizeof(buffer));
    if (bytes_read != strlen(data) || strcmp(buffer, data) != 0) {
        printf("FAILED: Data mismatch after write/read\n");
        return;
    }
    
    printf("PASSED: Empty file operations\n");
}

// Test 2: File size limits
void test_file_size_limits() {
    printf("=== Test 2: File Size Limits ===\n");
    
    // Test maximum file size (48KB = 12 blocks * 4KB)
    char large_data[LARGE_FILE_SIZE];
    for (int i = 0; i < LARGE_FILE_SIZE; i++) {
        large_data[i] = 'A' + (i % 26);
    }
    
    if (fs_create("large.txt") != 0) {
        printf("FAILED: Could not create large file\n");
        return;
    }
    
    if (fs_write("large.txt", large_data, LARGE_FILE_SIZE) != 0) {
        printf("FAILED: Could not write large file\n");
        return;
    }
    
    char read_buffer[LARGE_FILE_SIZE];
    int bytes_read = fs_read("large.txt", read_buffer, LARGE_FILE_SIZE);
    if (bytes_read != LARGE_FILE_SIZE) {
        printf("FAILED: Large file read returned %d, expected %d\n", bytes_read, LARGE_FILE_SIZE);
        return;
    }
    
    if (memcmp(large_data, read_buffer, LARGE_FILE_SIZE) != 0) {
        printf("FAILED: Large file data mismatch\n");
        return;
    }
    
    printf("PASSED: File size limits\n");
}

// Test 3: Error conditions
void test_error_conditions() {
    printf("=== Test 3: Error Conditions ===\n");
    
    // Test 1: Try to create file with too long name
    char long_name[50];
    memset(long_name, 'A', 49);
    long_name[49] = '\0';
    
    if (fs_create(long_name) != -3) {
        printf("FAILED: Should reject filename longer than 28 chars\n");
        return;
    }
    
    // Test 2: Try to read non-existent file
    char buffer[100];
    if (fs_read("nonexistent.txt", buffer, sizeof(buffer)) != -1) {
        printf("FAILED: Should return -1 for non-existent file\n");
        return;
    }
    
    // Test 3: Try to write to non-existent file
    char data[] = "test";
    if (fs_write("nonexistent.txt", data, strlen(data)) != -1) {
        printf("FAILED: Should return -1 for non-existent file\n");
        return;
    }
    
    // Test 4: Try to delete non-existent file
    if (fs_delete("nonexistent.txt") != -1) {
        printf("FAILED: Should return -1 for non-existent file\n");
        return;
    }
    
    // Test 5: Try to create duplicate file
    if (fs_create("test.txt") != 0) {
        printf("FAILED: Could not create test file\n");
        return;
    }
    if (fs_create("test.txt") != -1) {
        printf("FAILED: Should reject duplicate filename\n");
        return;
    }
    
    printf("PASSED: Error conditions\n");
}

// Test 4: File deletion and reuse
void test_file_deletion() {
    printf("=== Test 4: File Deletion and Reuse ===\n");
    
    // Create and write to file
    if (fs_create("delete_test.txt") != 0) {
        printf("FAILED: Could not create file for deletion test\n");
        return;
    }
    
    char data[] = "This file will be deleted";
    if (fs_write("delete_test.txt", data, strlen(data)) != 0) {
        printf("FAILED: Could not write to file\n");
        return;
    }
    
    // Delete the file
    if (fs_delete("delete_test.txt") != 0) {
        printf("FAILED: Could not delete file\n");
        return;
    }
    
    // Try to read deleted file
    char buffer[100];
    if (fs_read("delete_test.txt", buffer, sizeof(buffer)) != -1) {
        printf("FAILED: Should not be able to read deleted file\n");
        return;
    }
    
    // Create new file with same name
    if (fs_create("delete_test.txt") != 0) {
        printf("FAILED: Could not reuse filename after deletion\n");
        return;
    }
    
    printf("PASSED: File deletion and reuse\n");
}

// Test 5: Stress test - many small files
void test_many_small_files() {
    printf("=== Test 5: Many Small Files ===\n");
    
    char filename[30];
    char data[100];
    
    // Create many small files
    for (int i = 0; i < MAX_TEST_FILES; i++) {
        snprintf(filename, sizeof(filename), "small_%d.txt", i);
        snprintf(data, sizeof(data), "Data for file %d", i);
        
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create file %s\n", filename);
            return;
        }
        
        if (fs_write(filename, data, strlen(data)) != 0) {
            printf("FAILED: Could not write to file %s\n", filename);
            return;
        }
    }
    
    // List all files
    char file_list[MAX_TEST_FILES][MAX_FILENAME];
    int file_count = fs_list(file_list, MAX_TEST_FILES);
    if (file_count != MAX_TEST_FILES) {
        printf("FAILED: Expected %d files, got %d\n", MAX_TEST_FILES, file_count);
        return;
    }
    
    // Read back all files
    for (int i = 0; i < MAX_TEST_FILES; i++) {
        snprintf(filename, sizeof(filename), "small_%d.txt", i);
        snprintf(data, sizeof(data), "Data for file %d", i);
        
        char buffer[100];
        int bytes_read = fs_read(filename, buffer, sizeof(buffer));
        if (bytes_read != strlen(data) || strcmp(buffer, data) != 0) {
            printf("FAILED: Data mismatch for file %s\n", filename);
            return;
        }
    }
    
    printf("PASSED: Many small files\n");
}

// Test 6: Partial reads and writes
void test_partial_operations() {
    printf("=== Test 6: Partial Reads and Writes ===\n");
    
    // Create file with some data
    if (fs_create("partial.txt") != 0) {
        printf("FAILED: Could not create file\n");
        return;
    }
    
    char original_data[] = "This is a test file with some data";
    if (fs_write("partial.txt", original_data, strlen(original_data)) != 0) {
        printf("FAILED: Could not write original data\n");
        return;
    }
    
    // Read partial data
    char partial_buffer[10];
    int bytes_read = fs_read("partial.txt", partial_buffer, 10);
    if (bytes_read != 10) {
        printf("FAILED: Partial read returned %d, expected 10\n", bytes_read);
        return;
    }
    
    if (strncmp(partial_buffer, "This is a ", 10) != 0) {
        printf("FAILED: Partial read data mismatch\n");
        return;
    }
    
    // Overwrite with shorter data
    char shorter_data[] = "Short";
    if (fs_write("partial.txt", shorter_data, strlen(shorter_data)) != 0) {
        printf("FAILED: Could not write shorter data\n");
        return;
    }
    
    // Read back
    char full_buffer[100];
    bytes_read = fs_read("partial.txt", full_buffer, sizeof(full_buffer));
    if (bytes_read != strlen(shorter_data)) {
        printf("FAILED: Read after overwrite returned %d, expected %zu\n", 
               bytes_read, strlen(shorter_data));
        return;
    }
    
    if (strncmp(full_buffer, shorter_data, strlen(shorter_data)) != 0) {
        printf("FAILED: Data mismatch after overwrite\n");
        return;
    }
    
    printf("PASSED: Partial reads and writes\n");
}

// Test 7: Boundary conditions
void test_boundary_conditions() {
    printf("=== Test 7: Boundary Conditions ===\n");
    
    // Test exact block size
    char exact_block[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        exact_block[i] = 'X';
    }
    
    if (fs_create("exact_block.txt") != 0) {
        printf("FAILED: Could not create exact block file\n");
        return;
    }
    
    if (fs_write("exact_block.txt", exact_block, BLOCK_SIZE) != 0) {
        printf("FAILED: Could not write exact block size\n");
        return;
    }
    
    char read_block[BLOCK_SIZE];
    int bytes_read = fs_read("exact_block.txt", read_block, BLOCK_SIZE);
    if (bytes_read != BLOCK_SIZE) {
        printf("FAILED: Exact block read returned %d, expected %d\n", bytes_read, BLOCK_SIZE);
        return;
    }
    
    if (memcmp(exact_block, read_block, BLOCK_SIZE) != 0) {
        printf("FAILED: Exact block data mismatch\n");
        return;
    }
    
    // Test one byte less than block size
    char almost_block[BLOCK_SIZE - 1];
    for (int i = 0; i < BLOCK_SIZE - 1; i++) {
        almost_block[i] = 'Y';
    }
    
    if (fs_create("almost_block.txt") != 0) {
        printf("FAILED: Could not create almost block file\n");
        return;
    }
    
    if (fs_write("almost_block.txt", almost_block, BLOCK_SIZE - 1) != 0) {
        printf("FAILED: Could not write almost block size\n");
        return;
    }
    
    char read_almost[BLOCK_SIZE - 1];
    bytes_read = fs_read("almost_block.txt", read_almost, BLOCK_SIZE - 1);
    if (bytes_read != BLOCK_SIZE - 1) {
        printf("FAILED: Almost block read returned %d, expected %d\n", bytes_read, BLOCK_SIZE - 1);
        return;
    }
    
    if (memcmp(almost_block, read_almost, BLOCK_SIZE - 1) != 0) {
        printf("FAILED: Almost block data mismatch\n");
        return;
    }
    
    printf("PASSED: Boundary conditions\n");
}

int main() {
    printf("Starting Edge Case Tests...\n\n");
    
    setup_disk();
    
    test_empty_files();
    test_file_size_limits();
    test_error_conditions();
    test_file_deletion();
    test_many_small_files();
    test_partial_operations();
    test_boundary_conditions();
    
    fs_unmount();
    
    printf("\n=== All Edge Case Tests Completed ===\n");
    return 0;
} 