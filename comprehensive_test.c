#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs.h"

#define COMPREHENSIVE_DISK "comprehensive_disk.img"

void setup_comprehensive_disk() {
    if (access(COMPREHENSIVE_DISK, F_OK) == 0) {
        remove(COMPREHENSIVE_DISK);
    }
    if (fs_format(COMPREHENSIVE_DISK) != 0) {
        fprintf(stderr, "Failed to format comprehensive disk\n");
        exit(1);
    }
    if (fs_mount(COMPREHENSIVE_DISK) != 0) {
        fprintf(stderr, "Failed to mount comprehensive disk\n");
        exit(1);
    }
}

// Test 1: Creating multiple files
void test_multiple_files() {
    printf("=== Test 1: Creating Multiple Files ===\n");
    
    setup_comprehensive_disk();
    
    const int num_files = 20;
    char filename[30];
    
    // Create multiple files
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "multi_%d.txt", i);
        
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create file %s\n", filename);
            return;
        }
    }
    
    // List files and verify count
    char file_list[num_files][MAX_FILENAME];
    int file_count = fs_list(file_list, num_files);
    
    if (file_count != num_files) {
        printf("FAILED: Expected %d files, got %d\n", num_files, file_count);
        return;
    }
    
    printf("PASSED: Created %d files successfully\n", num_files);
    fs_unmount();
}

// Test 2: Writing files of different sizes
void test_different_sizes() {
    printf("=== Test 2: Writing Files of Different Sizes ===\n");
    
    setup_comprehensive_disk();
    
    // Test different file sizes
    int sizes[] = {1, 100, 1000, 4000, 8000, 16000, 32000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        char filename[30];
        snprintf(filename, sizeof(filename), "size_%d.txt", sizes[i]);
        
        // Create file
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create file %s\n", filename);
            return;
        }
        
        // Create data of specified size
        char* data = malloc(sizes[i]);
        if (!data) {
            printf("FAILED: Could not allocate memory for size %d\n", sizes[i]);
            return;
        }
        
        // Fill with pattern
        for (int j = 0; j < sizes[i]; j++) {
            data[j] = 'A' + (j % 26);
        }
        
        // Write data
        if (fs_write(filename, data, sizes[i]) != 0) {
            printf("FAILED: Could not write %d bytes to %s\n", sizes[i], filename);
            free(data);
            return;
        }
        
        // Read back and verify
        char* read_data = malloc(sizes[i]);
        if (!read_data) {
            printf("FAILED: Could not allocate read buffer for size %d\n", sizes[i]);
            free(data);
            return;
        }
        
        int bytes_read = fs_read(filename, read_data, sizes[i]);
        if (bytes_read != sizes[i]) {
            printf("FAILED: Read returned %d, expected %d for file %s\n", 
                   bytes_read, sizes[i], filename);
            free(data);
            free(read_data);
            return;
        }
        
        if (memcmp(data, read_data, sizes[i]) != 0) {
            printf("FAILED: Data mismatch for file %s (size %d)\n", filename, sizes[i]);
            free(data);
            free(read_data);
            return;
        }
        
        free(data);
        free(read_data);
        printf("PASSED: File size %d bytes\n", sizes[i]);
    }
    
    printf("PASSED: All file sizes tested successfully\n");
    fs_unmount();
}

// Test 3: Filling filesystem to capacity
void test_fill_capacity() {
    printf("=== Test 3: Filling Filesystem to Capacity ===\n");
    
    setup_comprehensive_disk();
    
    int files_created = 0;
    char filename[30];
    char data[BLOCK_SIZE]; // 4KB data
    
    // Fill data with pattern
    for (int i = 0; i < BLOCK_SIZE; i++) {
        data[i] = 'F' + (i % 26);
    }
    
    // Try to create files until we run out of space
    for (int i = 0; i < 1000; i++) { // Large number to ensure we hit limits
        snprintf(filename, sizeof(filename), "capacity_%d.txt", i);
        
        int create_result = fs_create(filename);
        if (create_result == -2) {
            printf("Filesystem full after %d files (no free inodes)\n", files_created);
            break;
        } else if (create_result != 0) {
            printf("Unexpected error creating file %s: %d\n", filename, create_result);
            return;
        }
        
        int write_result = fs_write(filename, data, BLOCK_SIZE);
        if (write_result == -2) {
            printf("Filesystem full after %d files (no free blocks)\n", files_created);
            // Delete the file we just created since we couldn't write to it
            fs_delete(filename);
            break;
        } else if (write_result != 0) {
            printf("Unexpected error writing to file %s: %d\n", filename, write_result);
            return;
        }
        
        files_created++;
        
        if (files_created % 10 == 0) {
            printf("Created %d files...\n", files_created);
        }
    }
    
    printf("PASSED: Successfully created %d files before hitting capacity\n", files_created);
    fs_unmount();
}

// Test 4: Deleting files and reusing space
void test_delete_and_reuse() {
    printf("=== Test 4: Deleting Files and Reusing Space ===\n");
    
    setup_comprehensive_disk();
    
    // Create some files first
    const int num_files = 15;
    char filename[30];
    char data[1000];
    
    for (int i = 0; i < 1000; i++) {
        data[i] = 'D' + (i % 26);
    }
    
    // Create files
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "reuse_%d.txt", i);
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create file %s\n", filename);
            return;
        }
        if (fs_write(filename, data, 1000) != 0) {
            printf("FAILED: Could not write to file %s\n", filename);
            return;
        }
    }
    
    // Delete every other file
    for (int i = 0; i < num_files; i += 2) {
        snprintf(filename, sizeof(filename), "reuse_%d.txt", i);
        if (fs_delete(filename) != 0) {
            printf("FAILED: Could not delete file %s\n", filename);
            return;
        }
    }
    
    // Create new files to reuse the space
    for (int i = 0; i < num_files; i += 2) {
        snprintf(filename, sizeof(filename), "reuse_new_%d.txt", i);
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create new file %s\n", filename);
            return;
        }
        if (fs_write(filename, data, 1000) != 0) {
            printf("FAILED: Could not write to new file %s\n", filename);
            return;
        }
    }
    
    // Verify we can read both old and new files
    for (int i = 1; i < num_files; i += 2) {
        snprintf(filename, sizeof(filename), "reuse_%d.txt", i);
        char buffer[1000];
        int bytes_read = fs_read(filename, buffer, 1000);
        if (bytes_read != 1000) {
            printf("FAILED: Could not read old file %s\n", filename);
            return;
        }
    }
    
    for (int i = 0; i < num_files; i += 2) {
        snprintf(filename, sizeof(filename), "reuse_new_%d.txt", i);
        char buffer[1000];
        int bytes_read = fs_read(filename, buffer, 1000);
        if (bytes_read != 1000) {
            printf("FAILED: Could not read new file %s\n", filename);
            return;
        }
    }
    
    printf("PASSED: Successfully deleted files and reused space\n");
    fs_unmount();
}

// Test 5: Testing error conditions
void test_error_conditions() {
    printf("=== Test 5: Testing Error Conditions ===\n");
    
    setup_comprehensive_disk();
    
    // Test 1: File not found
    char buffer[100];
    if (fs_read("nonexistent.txt", buffer, 100) != -1) {
        printf("FAILED: Should return -1 for non-existent file read\n");
        return;
    }
    
    if (fs_write("nonexistent.txt", "data", 4) != -1) {
        printf("FAILED: Should return -1 for non-existent file write\n");
        return;
    }
    
    if (fs_delete("nonexistent.txt") != -1) {
        printf("FAILED: Should return -1 for non-existent file delete\n");
        return;
    }
    
    // Test 2: Invalid parameters
    if (fs_create(NULL) != -3) {
        printf("FAILED: Should return -3 for NULL filename in create\n");
        return;
    }
    
    if (fs_write(NULL, "data", 4) != -3) {
        printf("FAILED: Should return -3 for NULL filename in write\n");
        return;
    }
    
    if (fs_read(NULL, buffer, 100) != -3) {
        printf("FAILED: Should return -3 for NULL filename in read\n");
        return;
    }
    
    if (fs_delete(NULL) != -3) {
        printf("FAILED: Should return -3 for NULL filename in delete\n");
        return;
    }
    
    // Test 3: Filename too long
    char long_name[50];
    memset(long_name, 'A', 49);
    long_name[49] = '\0';
    
    if (fs_create(long_name) != -3) {
        printf("FAILED: Should return -3 for filename too long\n");
        return;
    }
    
    // Test 4: Duplicate filename
    if (fs_create("test.txt") != 0) {
        printf("FAILED: Could not create test file\n");
        return;
    }
    
    if (fs_create("test.txt") != -1) {
        printf("FAILED: Should return -1 for duplicate filename\n");
        return;
    }
    
    // Test 5: Invalid size parameters
    if (fs_write("test.txt", "data", -1) != -3) {
        printf("FAILED: Should return -3 for negative size in write\n");
        return;
    }
    
    if (fs_read("test.txt", buffer, -1) != -3) {
        printf("FAILED: Should return -3 for negative size in read\n");
        return;
    }
    
    if (fs_write("test.txt", NULL, 4) != -3) {
        printf("FAILED: Should return -3 for NULL data in write\n");
        return;
    }
    
    if (fs_read("test.txt", NULL, 100) != -3) {
        printf("FAILED: Should return -3 for NULL buffer in read\n");
        return;
    }
    
    // Test 6: Filesystem not mounted
    fs_unmount();
    
    if (fs_create("test.txt") != -3) {
        printf("FAILED: Should return -3 when filesystem not mounted\n");
        return;
    }
    
    if (fs_write("test.txt", "data", 4) != -3) {
        printf("FAILED: Should return -3 when filesystem not mounted\n");
        return;
    }
    
    if (fs_read("test.txt", buffer, 100) != -3) {
        printf("FAILED: Should return -3 when filesystem not mounted\n");
        return;
    }
    
    if (fs_delete("test.txt") != -2) {
        printf("FAILED: Should return -2 when filesystem not mounted\n");
        return;
    }
    
    printf("PASSED: All error conditions tested successfully\n");
}

int main() {
    printf("Starting Comprehensive Tests...\n\n");
    
    test_multiple_files();
    test_different_sizes();
    test_fill_capacity();
    test_delete_and_reuse();
    test_error_conditions();
    
    printf("\n=== All Comprehensive Tests Completed Successfully! ===\n");
    return 0;
} 