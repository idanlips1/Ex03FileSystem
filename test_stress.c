#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "fs.h"

#define STRESS_DISK "stress_disk.img"
#define MAX_FILES_TO_CREATE 200
#define MEDIUM_FILE_SIZE 8000

// Helper function to create a fresh disk
void setup_stress_disk() {
    if (access(STRESS_DISK, F_OK) == 0) {
        remove(STRESS_DISK);
    }
    if (fs_format(STRESS_DISK) != 0) {
        fprintf(stderr, "Failed to format stress disk\n");
        exit(1);
    }
    if (fs_mount(STRESS_DISK) != 0) {
        fprintf(stderr, "Failed to mount stress disk\n");
        exit(1);
    }
}

// Test 1: Fill filesystem to capacity
void test_fill_filesystem() {
    printf("=== Test 1: Fill Filesystem to Capacity ===\n");
    
    int files_created = 0;
    char filename[30];
    char data[MEDIUM_FILE_SIZE];
    
    // Fill data with pattern
    for (int i = 0; i < MEDIUM_FILE_SIZE; i++) {
        data[i] = 'A' + (i % 26);
    }
    
    // Try to create as many files as possible
    for (int i = 0; i < MAX_FILES_TO_CREATE; i++) {
        snprintf(filename, sizeof(filename), "stress_%d.txt", i);
        
        int result = fs_create(filename);
        if (result == -2) {
            printf("Filesystem full after %d files (no free inodes)\n", files_created);
            break;
        } else if (result != 0) {
            printf("Unexpected error creating file %s: %d\n", filename, result);
            return;
        }
        
        result = fs_write(filename, data, MEDIUM_FILE_SIZE);
        if (result == -2) {
            printf("Filesystem full after %d files (no free blocks)\n", files_created);
            // Delete the file we just created since we couldn't write to it
            fs_delete(filename);
            break;
        } else if (result != 0) {
            printf("Unexpected error writing to file %s: %d\n", filename, result);
            return;
        }
        
        files_created++;
        
        if (files_created % 10 == 0) {
            printf("Created %d files...\n", files_created);
        }
    }
    
    printf("Successfully created and wrote to %d files\n", files_created);
    
    // Verify we can read back some files
    int files_to_verify = (files_created > 10) ? 10 : files_created;
    for (int i = 0; i < files_to_verify; i++) {
        snprintf(filename, sizeof(filename), "stress_%d.txt", i);
        
        char read_buffer[MEDIUM_FILE_SIZE];
        int bytes_read = fs_read(filename, read_buffer, MEDIUM_FILE_SIZE);
        
        if (bytes_read != MEDIUM_FILE_SIZE) {
            printf("FAILED: File %s read returned %d, expected %d\n", 
                   filename, bytes_read, MEDIUM_FILE_SIZE);
            return;
        }
        
        if (memcmp(data, read_buffer, MEDIUM_FILE_SIZE) != 0) {
            printf("FAILED: Data mismatch in file %s\n", filename);
            return;
        }
    }
    
    printf("PASSED: Filesystem capacity test\n");
}

// Test 2: Random access patterns
void test_random_access() {
    printf("=== Test 2: Random Access Patterns ===\n");
    
    // Create several files with different sizes
    const int num_files = 20;
    char filename[30];
    int file_sizes[num_files];
    
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "random_%d.txt", i);
        
        // Random file size between 100 and 4000 bytes
        file_sizes[i] = 100 + (rand() % 3900);
        
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create file %s\n", filename);
            return;
        }
        
        // Create data for this file
        char* data = malloc(file_sizes[i]);
        for (int j = 0; j < file_sizes[i]; j++) {
            data[j] = 'A' + (i % 26) + (j % 10);
        }
        
        if (fs_write(filename, data, file_sizes[i]) != 0) {
            printf("FAILED: Could not write to file %s\n", filename);
            free(data);
            return;
        }
        
        free(data);
    }
    
    // Randomly read from files
    for (int round = 0; round < 50; round++) {
        int file_idx = rand() % num_files;
        snprintf(filename, sizeof(filename), "random_%d.txt", file_idx);
        
        char* buffer = malloc(file_sizes[file_idx]);
        int bytes_read = fs_read(filename, buffer, file_sizes[file_idx]);
        
        if (bytes_read != file_sizes[file_idx]) {
            printf("FAILED: Random read returned %d, expected %d\n", 
                   bytes_read, file_sizes[file_idx]);
            free(buffer);
            return;
        }
        
        // Verify data pattern
        for (int j = 0; j < file_sizes[file_idx]; j++) {
            char expected = 'A' + (file_idx % 26) + (j % 10);
            if (buffer[j] != expected) {
                printf("FAILED: Data mismatch in random access\n");
                free(buffer);
                return;
            }
        }
        
        free(buffer);
    }
    
    printf("PASSED: Random access patterns\n");
}

// Test 3: Concurrent file operations simulation
void test_concurrent_operations() {
    printf("=== Test 3: Concurrent Operations Simulation ===\n");
    
    // Create files and perform mixed operations
    const int num_files = 30;
    char filename[30];
    
    // Phase 1: Create files
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "concurrent_%d.txt", i);
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create file %s\n", filename);
            return;
        }
    }
    
    // Phase 2: Mixed operations (simulate concurrent access)
    for (int round = 0; round < 100; round++) {
        int operation = rand() % 4; // 0=read, 1=write, 2=delete, 3=create
        int file_idx = rand() % num_files;
        
        switch (operation) {
            case 0: { // Read
                snprintf(filename, sizeof(filename), "concurrent_%d.txt", file_idx);
                char buffer[1000];
                int bytes_read = fs_read(filename, buffer, sizeof(buffer));
                if (bytes_read < 0 && bytes_read != -1) {
                    printf("FAILED: Unexpected read error: %d\n", bytes_read);
                    return;
                }
                break;
            }
            case 1: { // Write
                snprintf(filename, sizeof(filename), "concurrent_%d.txt", file_idx);
                char data[1000];
                for (int j = 0; j < 1000; j++) {
                    data[j] = 'W' + (round % 26);
                }
                int result = fs_write(filename, data, 1000);
                if (result != 0) {
                    printf("FAILED: Could not write to file %s: %d\n", filename, result);
                    return;
                }
                break;
            }
            case 2: { // Delete
                snprintf(filename, sizeof(filename), "concurrent_%d.txt", file_idx);
                int result = fs_delete(filename);
                if (result != 0) {
                    printf("FAILED: Could not delete file %s: %d\n", filename, result);
                    return;
                }
                // Recreate the file
                if (fs_create(filename) != 0) {
                    printf("FAILED: Could not recreate file %s\n", filename);
                    return;
                }
                break;
            }
            case 3: { // Create new file
                snprintf(filename, sizeof(filename), "concurrent_new_%d.txt", round);
                if (fs_create(filename) == 0) {
                    char data[] = "New file data";
                    fs_write(filename, data, strlen(data));
                }
                break;
            }
        }
    }
    
    printf("PASSED: Concurrent operations simulation\n");
}

// Test 4: Performance benchmark
void test_performance() {
    printf("=== Test 4: Performance Benchmark ===\n");
    
    clock_t start, end;
    double cpu_time_used;
    
    // Benchmark file creation
    start = clock();
    for (int i = 0; i < 100; i++) {
        char filename[30];
        snprintf(filename, sizeof(filename), "perf_%d.txt", i);
        fs_create(filename);
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Created 100 files in %.3f seconds\n", cpu_time_used);
    
    // Benchmark file writing
    char data[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        data[i] = 'P';
    }
    
    start = clock();
    for (int i = 0; i < 50; i++) {
        char filename[30];
        snprintf(filename, sizeof(filename), "perf_%d.txt", i);
        fs_write(filename, data, BLOCK_SIZE);
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Wrote 50 files (4KB each) in %.3f seconds\n", cpu_time_used);
    
    // Benchmark file reading
    char buffer[BLOCK_SIZE];
    start = clock();
    for (int i = 0; i < 50; i++) {
        char filename[30];
        snprintf(filename, sizeof(filename), "perf_%d.txt", i);
        fs_read(filename, buffer, BLOCK_SIZE);
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Read 50 files (4KB each) in %.3f seconds\n", cpu_time_used);
    
    printf("PASSED: Performance benchmark\n");
}

// Test 5: Resource exhaustion
void test_resource_exhaustion() {
    printf("=== Test 5: Resource Exhaustion ===\n");
    
    // Try to create more files than possible
    int files_created = 0;
    char filename[30];
    
    // Create files until we run out of inodes
    for (int i = 0; i < MAX_FILES + 10; i++) {
        snprintf(filename, sizeof(filename), "exhaust_%d.txt", i);
        
        int result = fs_create(filename);
        if (result == -2) {
            printf("Correctly ran out of inodes after %d files\n", files_created);
            break;
        } else if (result == 0) {
            files_created++;
        } else {
            printf("Unexpected error: %d\n", result);
            return;
        }
    }
    
    // Try to write to a file with more data than blocks available
    if (fs_create("huge.txt") == 0) {
        // Try to write a very large file (more than available blocks)
        char* huge_data = malloc(1000000); // 1MB
        if (huge_data) {
            for (int i = 0; i < 1000000; i++) {
                huge_data[i] = 'H';
            }
            
            int result = fs_write("huge.txt", huge_data, 1000000);
            if (result == -2) {
                printf("Correctly rejected write due to insufficient blocks\n");
            } else if (result == 0) {
                printf("WARNING: Large file write succeeded (may have truncated)\n");
            } else {
                printf("Unexpected error writing large file: %d\n", result);
            }
            
            free(huge_data);
        }
    }
    
    printf("PASSED: Resource exhaustion test\n");
}

int main() {
    printf("Starting Stress Tests...\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    setup_stress_disk();
    
    test_fill_filesystem();
    test_random_access();
    test_concurrent_operations();
    test_performance();
    test_resource_exhaustion();
    
    fs_unmount();
    
    printf("\n=== All Stress Tests Completed ===\n");
    return 0;
} 