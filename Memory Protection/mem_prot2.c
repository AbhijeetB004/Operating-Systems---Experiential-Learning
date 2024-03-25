/**
 * The code implements kernel-level memory management functions including handling page faults, memory
 * protection violations, and setting page permissions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h> // For concurrency and synchronization
#include <unistd.h>  // For system calls

// Define kernel-specific data structures and constants
#define NUM_PAGES 1024
#define TLB_SIZE 16
#define PAGE_SIZE 4096
#define NUM_FRAMES 1024
#define FRAME_SIZE 4096
#define BACKING_STORE "./BACKING_STORE.bin"

// Define kernel-specific memory protection flags
#define PERMISSION_READ 0x01
#define PERMISSION_WRITE 0x02
#define PERMISSION_EXECUTE 0x04

// Define kernel-level data structures
typedef struct PageTableEntry {
    bool valid_bit;           // 0: Page not loaded, 1: Page loaded
    bool dirty_bit;           // 0: Page not modified, 1: Page modified
    int frame_number;         // -1 if not loaded yet
    time_t last_accessed_time;
    int permissions;          // Access permissions for the page
} PageTableEntry;

// Kernel-level data structures
PageTableEntry page_table[NUM_PAGES];
pthread_mutex_t page_table_lock; // Mutex for page table access
char physical_memory[NUM_FRAMES][FRAME_SIZE];


// Function prototypes
void initialize_page_table();
void set_page_permissions(int virtual_page_number, int permissions);
int check_permissions(int virtual_address, int access_type);
void handle_fault_or_violation(int error_code, int virtual_page_number);
int handle_page_fault(int virtual_page_number);


void initialize_page_table() {
    pthread_mutex_init(&page_table_lock, NULL);
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i].valid_bit = false;
        page_table[i].dirty_bit = false;
        page_table[i].frame_number = -1;
        page_table[i].last_accessed_time = 0;
        page_table[i].permissions = PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXECUTE;
    }
}


void set_page_permissions(int virtual_page_number, int permissions) {
    pthread_mutex_lock(&page_table_lock);
    if (virtual_page_number >= 0 && virtual_page_number < NUM_PAGES) {
        page_table[virtual_page_number].permissions = permissions;
        page_table[virtual_page_number].valid_bit = true;
    } else {
        fprintf(stderr, "Invalid virtual page number: %d\n", virtual_page_number);
    }
    pthread_mutex_unlock(&page_table_lock);
}

PageTableEntry *get_page_table_entry(int virtual_page_number) {
    if (virtual_page_number < 0 || virtual_page_number >= NUM_PAGES) {
        fprintf(stderr, "Invalid virtual page number: %d\n", virtual_page_number);
        return NULL;
    }
    
    return &page_table[virtual_page_number];
}

void set_page_table_entry(int virtual_page_number, int valid_bit, int dirty_bit, int frame_number) {
    PageTableEntry *entry = get_page_table_entry(virtual_page_number);
    if (entry) {
        entry->valid_bit = valid_bit;
        entry->dirty_bit = dirty_bit;
        entry->frame_number = frame_number;
        entry->last_accessed_time = time(NULL); // Update the last accessed time to the current time
    }
}

int check_permissions(int virtual_address, int access_type) {
    int virtual_page_number = virtual_address / PAGE_SIZE;
    if (virtual_page_number < 0 || virtual_page_number >= NUM_PAGES) {
        fprintf(stderr, "Invalid virtual page number: %d\n", virtual_page_number);
        return -1; // Invalid virtual page number
    }
    pthread_mutex_lock(&page_table_lock);
    PageTableEntry *entry = &page_table[virtual_page_number];
    if (!entry->valid_bit) {
        pthread_mutex_unlock(&page_table_lock);
        return -2; // Page fault
    }
    int required_permissions;
    switch (access_type) {
        case PERMISSION_READ:
            required_permissions = PERMISSION_READ;
            break;
        case PERMISSION_WRITE:
            required_permissions = PERMISSION_WRITE;
            break;
        case PERMISSION_EXECUTE:
            required_permissions = PERMISSION_EXECUTE;
            break;
        default:
            pthread_mutex_unlock(&page_table_lock);
            fprintf(stderr, "Invalid access type\n");
            return -3; // Invalid access type
    }
    if ((entry->permissions & required_permissions) != required_permissions) {
        pthread_mutex_unlock(&page_table_lock);
        return -4; // Memory protection violation
    }
    pthread_mutex_unlock(&page_table_lock);
    return 0; // Access allowed
}
int allocate_frame() {
    // For now we used a static counter 
    // But developers can implement their own allocation logic
    static int next_frame = 0;
    if (next_frame < NUM_FRAMES) {
        return next_frame++;
    } else {
        return -1; // No free frame available
    }
}

// Function to read a page from the backing store into the allocated frame
int read_from_backing_store(int virtual_page_number, int frame_number) {
    FILE *backing_store_fp = fopen(BACKING_STORE, "rb");
    if (backing_store_fp == NULL) {
        fprintf(stderr, "Error opening backing store file.\n");
        return -1;
    }
    // Seek to the position of the page in the backing store
    if (fseek(backing_store_fp, virtual_page_number * PAGE_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store file.\n");
        fclose(backing_store_fp);
        return -1;
    }
    // Read the page from the backing store into the allocated frame
    if (fread(physical_memory[frame_number], sizeof(char), FRAME_SIZE, backing_store_fp) != FRAME_SIZE) {
        fprintf(stderr, "Error reading from backing store file.\n");
        fclose(backing_store_fp);
        return -1;
    }
    // Close the backing store file
    fclose(backing_store_fp);
    return 0;
}




// Implement kernel-level memory management functions using appropriate data structures and algorithms

int handle_page_fault(int virtual_page_number) {
    // Allocate a frame in physical memory
    int frame_number = allocate_frame();
    if (frame_number == -1) {
        fprintf(stderr, "No free frame available in physical memory.\n");
        return -1;
    }
    // Read the page from the backing store into the allocated frame
    if (read_from_backing_store(virtual_page_number, frame_number) != 0) {
        fprintf(stderr, "Error reading from backing store.\n");
        return -1;
    }
    // Update the page table entry for the required page
    set_page_table_entry(virtual_page_number, 1, 0, frame_number);
    return 0;
}

void handle_fault_or_violation(int error_code, int virtual_page_number) {
    switch (error_code) {
        case -2:
            // Page fault has occurred
            // Devs can implement their own handling mechanism
            handle_page_fault(virtual_page_number);
            break;
        case -4:
            // Memory protection violation
            // Handle violation (e.g., terminate process)
            fprintf(stderr, "Memory protection violation! Virtual address: %d\n", virtual_page_number * PAGE_SIZE);
            break;
        default:
            fprintf(stderr, "Unknown error code\n");
            break;
    }
}
// Integrate memory management functionality with kernel APIs and subsystems

// Handle concurrency, synchronization, error handling, and security considerations

int main() {
    // Initialize page table
    initialize_page_table();

    // Example: Set access permissions for virtual page 0 as read-only
    set_page_permissions(0, PERMISSION_READ);

    // Example: Perform memory access and check permissions
    int virtual_address = 0;
    int access_type = PERMISSION_WRITE;
    int result = check_permissions(virtual_address, access_type);
    if (result == 0) {
        // Memory access allowed
        printf("Memory access allowed\n");
    } else {
        // Memory access not allowed, handle error
        handle_fault_or_violation(result, virtual_address);
    }

    return 0;
}
