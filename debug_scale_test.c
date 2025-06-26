#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs.h"

#define SCALE_DISK "scale_disk.img"

void setup_scale_disk() {
    if (access(SCALE_DISK, F_OK) == 0) {
        remove(SCALE_DISK);
    }
    if (fs_format(SCALE_DISK) != 0) {
        fprintf(stderr, "Failed to format scale disk\n");
        exit(1);
    }
    if (fs_mount(SCALE_DISK) != 0) {
        fprintf(stderr, "Failed to mount scale disk\n");
        exit(1);
    }
}

void test_scale_files(int num_files) {
    printf("=== Testing with %d files ===\n", num_files);
    
    char filename[30];
    char data[100];
    
    // Create files and write data
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "scale_%d.txt", i);
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
    
    // Read back and verify
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "scale_%d.txt", i);
        snprintf(data, sizeof(data), "Data for file %d", i);
        
        char buffer[100];
        memset(buffer, 0, sizeof(buffer));
        
        int bytes_read = fs_read(filename, buffer, sizeof(buffer));
        
        if (bytes_read != strlen(data)) {
            printf("FAILED: Length mismatch for file %s (expected %zu, got %d)\n", 
                   filename, strlen(data), bytes_read);
            return;
        }
        
        if (strcmp(buffer, data) != 0) {
            printf("FAILED: Data mismatch for file %s\n", filename);
            printf("Expected: '%s'\n", data);
            printf("Got:      '%s'\n", buffer);
            return;
        }
    }
    
    printf("PASSED: All %d files work correctly\n", num_files);
}

int main() {
    printf("Starting Scale Test...\n\n");
    
    // Test with increasing numbers of files
    int test_sizes[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    for (int test = 0; test < num_tests; test++) {
        int num_files = test_sizes[test];
        
        // Setup fresh disk for each test
        setup_scale_disk();
        test_scale_files(num_files);
        fs_unmount();
    }
    
    printf("\n=== Scale Test Completed ===\n");
    return 0;
} 