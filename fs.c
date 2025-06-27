#include "fs.h"

// Global variables for in-memory filesystem state
static superblock sb;
static inode inode_table[MAX_FILES];
static unsigned char block_bitmap[BLOCK_SIZE];
static int disk_fd = -1;

// Helper function prototypes
static int find_inode(const char* filename);
static int find_free_inode();
static int find_free_block();
static void mark_block_used(int block_num);
static void mark_block_free(int block_num);
static void read_inode(int inode_num, inode* target);
static void write_inode(int inode_num, const inode* source);

// Find the index of the inode with the given filename, or -1 if not found
static int find_inode(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].used && strncmp(inode_table[i].name, filename, MAX_FILENAME) == 0) {
            return i;
        }
    }
    return -1;
}

// Find the index of a free inode, or -1 if none are free
static int find_free_inode() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!inode_table[i].used) {
            return i;
        }
    }
    return -1;
}

// Find the index of a free data block (blocks 10+), or -1 if none are free
static int find_free_block() {
    for (int i = 10; i < MAX_BLOCKS; i++) {
        int byte = i / 8;
        int bit = i % 8;
        if (!(block_bitmap[byte] & (1 << bit))) {
            return i;
        }
    }
    return -1;
}

// Mark a block as used in the bitmap
static void mark_block_used(int block_num) {
    int byte = block_num / 8;
    int bit = block_num % 8;
    block_bitmap[byte] |= (1 << bit);
}

// Mark a block as free in the bitmap
static void mark_block_free(int block_num) {
    int byte = block_num / 8;
    int bit = block_num % 8;
    block_bitmap[byte] &= ~(1 << bit);
}



int fs_format(const char* disk_path) {
    // Open or create the disk file
    int fd = open(disk_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    // Initialize superblock
    sb.total_blocks = MAX_BLOCKS;
    sb.block_size = BLOCK_SIZE;
    sb.free_blocks = MAX_BLOCKS - 10; // 10 metadata blocks used
    sb.total_inodes = MAX_FILES;
    sb.free_inodes = MAX_FILES;

    // Initialize block bitmap: set all to 0, then mark blocks 0-9 as used
    for (int i = 0; i < BLOCK_SIZE; i++) block_bitmap[i] = 0;
    for (int i = 0; i < 10; i++) mark_block_used(i);

    // Initialize inode table: mark all as unused and clear fields
    for (int i = 0; i < MAX_FILES; i++) {
        inode_table[i].used = 0;
        for (int j = 0; j < MAX_FILENAME; j++) inode_table[i].name[j] = 0;
        inode_table[i].size = 0;
        for (int j = 0; j < MAX_DIRECT_BLOCKS; j++) inode_table[i].blocks[j] = 0;
    }

    // Write superblock to block 0
    lseek(fd, 0, SEEK_SET);
    write(fd, &sb, sizeof(sb));
    // Pad the rest of block 0
    char zero_buf[BLOCK_SIZE] = {0};
    write(fd, zero_buf, BLOCK_SIZE - sizeof(sb));

    // Write block bitmap to block 1
    lseek(fd, BLOCK_SIZE * 1, SEEK_SET);
    write(fd, block_bitmap, BLOCK_SIZE);

    // Write inode table to blocks 2-9
    lseek(fd, BLOCK_SIZE * 2, SEEK_SET);
    write(fd, inode_table, sizeof(inode_table));

    // Fill the rest of the disk file with zeros (data blocks)
    lseek(fd, BLOCK_SIZE * 10, SEEK_SET);
    for (int i = 0; i < (MAX_BLOCKS - 10); i++) {
        write(fd, zero_buf, BLOCK_SIZE);
    }

    close(fd);
    return 0;
}

int fs_mount(const char* disk_path) {
    if (disk_fd != -1) return -1; // Already mounted
    disk_fd = open(disk_path, O_RDWR);
    if (disk_fd < 0) return -1;

    // Read superblock
    lseek(disk_fd, 0, SEEK_SET);
    if (read(disk_fd, &sb, sizeof(sb)) != sizeof(sb)) {
        close(disk_fd); disk_fd = -1; return -1;
    }

    // Validate superblock fields
    if (sb.total_blocks != MAX_BLOCKS || sb.block_size != BLOCK_SIZE ||
        sb.total_inodes != MAX_FILES) {
        close(disk_fd); disk_fd = -1; return -1;
    }

    // Read block bitmap
    lseek(disk_fd, BLOCK_SIZE * 1, SEEK_SET);
    if (read(disk_fd, block_bitmap, BLOCK_SIZE) != BLOCK_SIZE) {
        close(disk_fd); disk_fd = -1; return -1;
    }

    // Read inode table
    lseek(disk_fd, BLOCK_SIZE * 2, SEEK_SET);
    if (read(disk_fd, inode_table, sizeof(inode_table)) != sizeof(inode_table)) {
        close(disk_fd); disk_fd = -1; return -1;
    }

    return 0;
}

void fs_unmount() {
    if (disk_fd == -1) return; // Not mounted

    // Write superblock back to disk
    lseek(disk_fd, 0, SEEK_SET);
    write(disk_fd, &sb, sizeof(sb));

    // Write block bitmap back to disk
    lseek(disk_fd, BLOCK_SIZE * 1, SEEK_SET);
    write(disk_fd, block_bitmap, BLOCK_SIZE);

    // Write inode table back to disk
    lseek(disk_fd, BLOCK_SIZE * 2, SEEK_SET);
    write(disk_fd, inode_table, sizeof(inode_table));

    // Close the disk file and reset state
    close(disk_fd);
    disk_fd = -1;
}


int fs_create(const char* filename) {
    // Per fs.h, -3 is for "other errors" like the FS not being mounted.
    if (disk_fd == -1) return -3; 

    // Check for NULL filename
    if (!filename) return -3;

    // A filename of MAX_FILENAME length would not fit with a null terminator.
    if (strlen(filename) >= MAX_FILENAME) return -3; 
    
    // Check if filename already exists.
    if (find_inode(filename) != -1) return -1;

    // Find a free inode
    int inode_idx = find_free_inode();
    if (inode_idx == -1) return -2; // No free inodes available

    // Initialize the inode
    inode* new_inode = &inode_table[inode_idx];
    new_inode->used = 1;
    strncpy(new_inode->name, filename, MAX_FILENAME);
    new_inode->name[MAX_FILENAME - 1] = '\0'; // Ensure null termination
    new_inode->size = 0;
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        new_inode->blocks[i] = 0; // No data blocks allocated yet
    }

    // Update the superblock
    sb.free_inodes--;

    return 0; // Success
}

int fs_delete(const char* filename) {
    // 1. Pre-condition Checks
    if (disk_fd == -1) return -2; // "Other errors" for not mounted

    // Check for NULL filename
    if (!filename) return -3;

    // check if the filename is valid
    if (strlen(filename) >= MAX_FILENAME) return -3; 

    // Find the file's inode
    int inode_idx = find_inode(filename);
    if (inode_idx == -1) return -1; // File doesn't exist

    inode* target_inode = &inode_table[inode_idx];

    // 2. Mark all of the file's blocks as free in the bitmap
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (target_inode->blocks[i] != 0) {
            mark_block_free(target_inode->blocks[i]);
            sb.free_blocks++; // Update superblock
            target_inode->blocks[i] = 0; // Clear the block pointer in the inode
        }
    }

    // 3. Mark the inode as free
    target_inode->used = 0;
    target_inode->name[0] = '\0'; // Clear name
    target_inode->size = 0;

    // 4. Update the superblock's free inode count
    sb.free_inodes++;

    return 0; // Success
}
int fs_list(char filenames[][MAX_FILENAME], int max_files) {
   // check if the filesystem is mounted and the parameters are valid
    if (disk_fd == -1 || !filenames || max_files <= 0 || max_files > MAX_FILES) return -1; 

    int count = 0;
    for (int i = 0; i < MAX_FILES && count < max_files; i++) {
        if (inode_table[i].used) {
            strncpy(filenames[count], inode_table[i].name, MAX_FILENAME);
            filenames[count][MAX_FILENAME - 1] = '\0'; // Ensure null termination
            count++;
        }
    }
    return count;
}
int fs_write(const char* filename, const void* data, int size) {
    // check if the filesystem is mounted and the parameters are valid
    if (disk_fd == -1 || !filename || !data || size <= 0) return -3;

    // check if the filename is valid
    if (strlen(filename) >= MAX_FILENAME) return -3; 

    // check if the file is too large
    if (size > MAX_FILES * BLOCK_SIZE) return -3;

    // Find the inode for the file
    int inode_idx = find_inode(filename);
    if (inode_idx == -1) return -1; // File doesn't exist

    inode* target_inode = &inode_table[inode_idx];

    // Calculate the number of blocks needed
    int needed_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // Check if there's enough space
    if (sb.free_blocks < needed_blocks) return -2; // "Out of space"

    // Free old blocks
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (target_inode->blocks[i] != 0) {
            mark_block_free(target_inode->blocks[i]);
            sb.free_blocks++;
            target_inode->blocks[i] = 0;
        }
    }

    // Allocate new blocks
    const char* data_ptr = (const char*)data;
    for (int i = 0; i < needed_blocks; i++) {
        int block_idx = find_free_block();
       
        mark_block_used(block_idx);
        sb.free_blocks--;
        target_inode->blocks[i] = block_idx;

        lseek(disk_fd, block_idx * BLOCK_SIZE, SEEK_SET);
        int to_write;
        // If the last block, write the remaining data. Otherwise, write a full block.
        if (i == needed_blocks - 1) {
            to_write = size - i * BLOCK_SIZE;
        } else {
            to_write = BLOCK_SIZE;
        }
        write(disk_fd, data_ptr + i * BLOCK_SIZE, to_write);

        // Zero the rest of the block if it's not a full block
        if (to_write < BLOCK_SIZE) {
            // Zero the rest of the block
            char zero_buf[BLOCK_SIZE] = {0};
            write(disk_fd, zero_buf, BLOCK_SIZE - to_write);
        }
    }
    // Zero out unused block pointers
    for (int i = needed_blocks; i < MAX_DIRECT_BLOCKS; i++) target_inode->blocks[i] = 0;
    // update the inode's size
    target_inode->size = size;
    return 0; // Success
}
int fs_read(const char* filename, void* data, int size) {
    // check if the filesystem is mounted and the parameters are valid
    if (disk_fd == -1 || !filename || !data || size <= 0) return -3;

    // check if the filename is valid
    if (strlen(filename) >= MAX_FILENAME) return -3;

    // Find the inode for the file
    int inode_idx = find_inode(filename);
    if (inode_idx == -1) return -1; // File doesn't exist

    inode* target_inode = &inode_table[inode_idx];
    // Determine the number of bytes to read (min of size and file size)
    int bytes_to_read;
    if (size > target_inode->size) {
        bytes_to_read = target_inode->size;
    } else {
        bytes_to_read = size;
    }

    // count the number of bytes read
    int bytes_read = 0;
    char* data_ptr = (char*)data;
    for (int i = 0; i < MAX_DIRECT_BLOCKS && bytes_read < bytes_to_read; i++) {
        int block_idx = target_inode->blocks[i];
        if (block_idx == 0) break; // No more blocks to

        lseek(disk_fd, block_idx * BLOCK_SIZE, SEEK_SET);
        int chunk;
        if (bytes_to_read - bytes_read < BLOCK_SIZE) {
            chunk = bytes_to_read - bytes_read;
        } else {
            chunk = BLOCK_SIZE;
        }
        int n = read(disk_fd, data_ptr + bytes_read, chunk);
        if (n < 0) return -3; // Read error
        bytes_read += n;
    }
    return bytes_read; // Success
}