#ifndef MMAP_API_H
#define MMAP_API_H

#include <stddef.h> // For size_t
#include <stdint.h> // For uint32_t
#include <stdbool.h> // For bool
#include <time.h>

/* Page table entry structure */
typedef struct PageTableEntry {
    bool valid_bit;           // 0: Page not loaded, 1: Page loaded
    bool dirty_bit;           // 0: Page not modified, 1: Page modified
    int frame_number;         // -1 if not loaded yet
    time_t last_accessed_time;
    int permissions;          // Access permissions for the page
} PageTableEntry;

/* Function declarations */

// Allocate memory and initialize to zero
void *xcalloc(char *struct_name, int units);

// Free dynamically allocated memory
void xfree(void *ptr);

// Allocate memory from shared memory pools
void *SM_alloc(size_t size);

// Deallocate memory allocated from shared memory pools
void SM_dealloc(void *ptr);




// Translate virtual address to physical address
int translate_address(int physical_address);

// Check if the virtual address has valid permissions
bool check_permissions(int virtual_address, int access_type);

// Handle page fault or memory protection violation
void handle_fault_or_violation(int error_code, int virtual_page_number);

// Initialize memory management system
void mm_init();

// Register a new page family for memory management
void mm_instantiate_new_page_family(char *struct_name, uint32_t struct_size);

// Print memory usage statistics for a specific page family
void mm_print_memory_usage(char *struct_name);

// Print all registered page families
void mm_print_registered_page_families();

// Print block usage statistics
void mm_print_block_usage();

#endif /* MMAP_API_H */