#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fs.h"

#define DEBUG_DISK "debug_disk.img"

void setup_debug_disk() {
    if (access(DEBUG_DISK, F_OK) == 0) {
        remove(DEBUG_DISK);
    }
    if (fs_format(DEBUG_DISK) != 0) {
        fprintf(stderr, "Failed to format debug disk\n");
        exit(1);
    }
    if (fs_mount(DEBUG_DISK) != 0) {
        fprintf(stderr, "Failed to mount debug disk\n");
        exit(1);
    }
}

void debug_small_files() {
    printf("=== Debug: Many Small Files Issue ===\n");
    
    // Test with just a few files first
    const int num_files = 5;
    char filename[30];
    char data[100];
    
    printf("Creating %d small files...\n", num_files);
    
    // Create files and write data
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "small_%d.txt", i);
        snprintf(data, sizeof(data), "Data for file %d", i);
        
        printf("Creating file: %s\n", filename);
        if (fs_create(filename) != 0) {
            printf("FAILED: Could not create file %s\n", filename);
            return;
        }
        
        printf("Writing data: '%s' (length: %zu)\n", data, strlen(data));
        if (fs_write(filename, data, strlen(data)) != 0) {
            printf("FAILED: Could not write to file %s\n", filename);
            return;
        }
    }
    
    printf("\nReading back files...\n");
    
    // Read back and verify
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "small_%d.txt", i);
        snprintf(data, sizeof(data), "Data for file %d", i);
        
        char buffer[100];
        memset(buffer, 0, sizeof(buffer));
        
        printf("Reading file: %s\n", filename);
        int bytes_read = fs_read(filename, buffer, sizeof(buffer));
        printf("Bytes read: %d\n", bytes_read);
        printf("Expected data: '%s' (length: %zu)\n", data, strlen(data));
        printf("Actual data: '%s' (length: %zu)\n", buffer, strlen(buffer));
        
        if (bytes_read != strlen(data)) {
            printf("FAILED: Length mismatch for file %s\n", filename);
            printf("Expected %zu bytes, got %d bytes\n", strlen(data), bytes_read);
            return;
        }
        
        if (strcmp(buffer, data) != 0) {
            printf("FAILED: Data mismatch for file %s\n", filename);
            printf("Expected: '%s'\n", data);
            printf("Got:      '%s'\n", buffer);
            
            // Show character-by-character comparison
            printf("Character comparison:\n");
            int max_len = (strlen(data) > strlen(buffer)) ? strlen(data) : strlen(buffer);
            for (int j = 0; j < max_len; j++) {
                if (j < strlen(data) && j < strlen(buffer)) {
                    if (data[j] != buffer[j]) {
                        printf("  Position %d: expected '%c' (0x%02x), got '%c' (0x%02x)\n", 
                               j, data[j], (unsigned char)data[j], buffer[j], (unsigned char)buffer[j]);
                    }
                } else if (j < strlen(data)) {
                    printf("  Position %d: expected '%c' (0x%02x), got end of string\n", 
                           j, data[j], (unsigned char)data[j]);
                } else {
                    printf("  Position %d: expected end of string, got '%c' (0x%02x)\n", 
                           j, buffer[j], (unsigned char)buffer[j]);
                }
            }
            return;
        }
        
        printf("PASSED: File %s\n", filename);
    }
    
    printf("All files read correctly!\n");
}

void debug_file_listing() {
    printf("\n=== Debug: File Listing ===\n");
    
    char file_list[10][MAX_FILENAME];
    int file_count = fs_list(file_list, 10);
    
    printf("Filesystem contains %d files:\n", file_count);
    for (int i = 0; i < file_count; i++) {
        printf("  %d: '%s'\n", i, file_list[i]);
    }
}

int main() {
    printf("Starting Debug Test...\n\n");
    
    setup_debug_disk();
    
    debug_small_files();
    debug_file_listing();
    
    fs_unmount();
    
    printf("\n=== Debug Test Completed ===\n");
    return 0;
} 