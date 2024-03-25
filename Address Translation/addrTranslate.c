/**
 * The code implements a virtual memory management system using page tables, TLB, and handling page
 * faults.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define NUM_PAGES 256  
#define TLB_SIZE 16      
#define PAGE_SIZE 256
#define NUM_FRAMES 256
#define FRAME_SIZE 256

#define BACKING_STORE "./BACKING_STORE.bin"
// Page Table Entry structure

/**
 * The PageTableEntry struct represents an entry in a page table with fields for validity, dirty
 * status, frame number, and last accessed time.
 * @property {int} valid_bit - The `valid_bit` in the `PageTableEntry` struct is used to indicate
 * whether a page is currently loaded in memory or not. A value of 0 typically means the page is not
 * loaded, while a value of 1 indicates that the page is loaded and accessible in memory.
 * @property {int} dirty_bit - The `dirty_bit` in the `PageTableEntry` struct is used to indicate
 * whether a page has been modified or not. A value of 0 typically means the page has not been
 * modified, while a value of 1 indicates that the page has been modified since it was loaded into
 * memory.
 * @property {int} frame_number - The `frame_number` property in the `PageTableEntry` struct represents
 * the physical frame number where the page is currently loaded in memory. If the page is not loaded
 * yet, the `frame_number` is typically set to -1.
 * @property {time_t} last_accessed_time - The `last_accessed_time` property in the `PageTableEntry`
 * struct represents the time at which the page was last accessed. This timestamp can be used to track
 * when a particular page was accessed for various purposes such as page replacement algorithms or
 * performance monitoring.
 */
typedef struct PageTableEntry {
    int valid_bit;       // 0: Page not loaded, 1: Page loaded
    int dirty_bit;       // 0: Page not modified, 1: Page modified
    int frame_number;    // -1 if not loaded yet
    time_t last_accessed_time;
} PageTableEntry;

// TLB Entry structure
/**
 * The TLBEntry struct represents an entry in a Translation Lookaside Buffer with virtual page number,
 * physical frame number, and last accessed time fields.
 * @property {int} virtual_page_number - The `virtual_page_number` property in the `TLBEntry` struct
 * represents the page number in the virtual memory address space. It is used to map a virtual page to
 * a physical frame in the TLB (Translation Lookaside Buffer) for efficient address translation.
 * @property {int} physical_frame_number - The `physical_frame_number` property in the `TLBEntry`
 * struct represents the physical frame number where the corresponding virtual page is stored in
 * memory. This mapping is used in virtual memory management to translate virtual addresses to physical
 * addresses.
 * @property {time_t} last_accessed_time - The `last_accessed_time` property in the `TLBEntry` struct
 * represents the time at which the TLB entry was last accessed. This can be useful for implementing
 * replacement algorithms or for tracking the usage patterns of the TLB entries. The `time_t` data type
 * is typically used to
 */
typedef struct TLBEntry {
    int virtual_page_number;
    int physical_frame_number;
    time_t last_accessed_time;
} TLBEntry;

// Page table array
PageTableEntry page_table[NUM_PAGES];

// TLB array
TLBEntry tlb[TLB_SIZE];

// Memory 
char **memory;


// Variables to track Page faults and TLB hits
int page_faults=0;
int tlb_hits = 0;


// Define an array to store the access times of each frame
int memory_access_times[NUM_FRAMES] = {0};




// initialize the page table with default values
void initialize_page_table() {
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i].valid_bit = 0;
        page_table[i].dirty_bit = 0;
        page_table[i].frame_number = -1;
         page_table[i].last_accessed_time = 0;
    }
}

// initialize the TLB with default values
void initialize_tlb() {
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].virtual_page_number = -1;
        tlb[i].physical_frame_number = -1;
        tlb[i].last_accessed_time = 0; 
    }
}

// returns the tlb entry itself
TLBEntry *get_tlb_entry(int virtual_page_number) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].virtual_page_number == virtual_page_number) {
            return &tlb[i];
        }
    }
    return NULL;
}

// retrieve a page table entry when we give virtual page number
/**
 * The function `get_page_table_entry` retrieves a page table entry for a given virtual page number,
 * checking the TLB first and handling invalid page numbers.
 * 
 * @param virtual_page_number The `virtual_page_number` parameter is an integer representing the number
 * of a virtual page in a paging system. It is used to look up the corresponding entry in the page
 * table.
 * 
 * @return A pointer to a PageTableEntry is being returned.
 */
PageTableEntry *get_page_table_entry(int virtual_page_number) {
    if (virtual_page_number < 0 || virtual_page_number >= NUM_PAGES) {
        fprintf(stderr, "Invalid virtual page number: %d\n", virtual_page_number);
        return NULL;
    }
    TLBEntry *tlb_entry = get_tlb_entry(virtual_page_number);
    if (tlb_entry) {
        tlb_hits++;
        return &page_table[tlb_entry->virtual_page_number];
    }
    return &page_table[virtual_page_number];
}

// Sets the table entry when we give virtual page number
void set_page_table_entry(int virtual_page_number, int valid_bit, int dirty_bit, int frame_number) {
    PageTableEntry *entry = get_page_table_entry(virtual_page_number);
    if (entry) {
        entry->valid_bit = valid_bit;
        entry->dirty_bit = dirty_bit;
        entry->frame_number = frame_number;
        entry->last_accessed_time = time(NULL); // Update the last accessed time to the current time
    }
}




// Function to add a TLB entry
void add_tlb_entry(int virtual_page_number, int physical_frame_number) {
    // idea is to find the oldest entry based on the last accessed time
    int oldest_index = 0;
    time_t oldest_time = tlb[0].last_accessed_time; // wee will assume that the first entry has the oldest time

    for (int i = 1; i < TLB_SIZE; i++) {
        if (tlb[i].last_accessed_time < oldest_time) {
            oldest_time = tlb[i].last_accessed_time;
            oldest_index = i;
        }
    }

    // Replacing
    tlb[oldest_index].virtual_page_number = virtual_page_number;
    tlb[oldest_index].physical_frame_number = physical_frame_number;
    tlb[oldest_index].last_accessed_time = time(NULL); // Update the last accessed time
}


int findLRUFrame() {
    int lru_frame = 0;
    int min_access_time = memory_access_times[0];

    // Find the frame with the minimum access time 
    for (int i = 1; i < NUM_FRAMES; i++) {
        if (memory_access_times[i] < min_access_time) {
            min_access_time = memory_access_times[i];
            lru_frame = i;
        }
    }

    // Update the access time of the least recently used frame
    memory_access_times[lru_frame]++;

    return lru_frame;
}


void handle_page_fault(int virtual_page_number) {
    // Read the page from the backing store
    FILE *backing_store_fp = fopen(BACKING_STORE, "rb");
    if (backing_store_fp == NULL) {
        fprintf(stderr, "Error opening backing store file.\n");
        exit(1);
    }

    // Seek to the position of the page in the backing store
    if (fseek(backing_store_fp, virtual_page_number * PAGE_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store file.\n");
        fclose(backing_store_fp);
        exit(1);
    }

    // Allocate a frame in physical memory
    int frame_number = findLRUFrame();
    if (frame_number == -1) {
        // Handle no free frame situation (consider error handling)
        fprintf(stderr, "No free frame available in physical memory.\n");
        fclose(backing_store_fp);
        exit(1);
    }

    // Read the page from the backing store into the allocated frame
    // if (fread(memory[frame_number], sizeof(char), PAGE_SIZE, backing_store_fp) != PAGE_SIZE) {
    //     fprintf(stderr, "Error reading from backing store file.\n");
    //     fclose(backing_store_fp);
    //     exit(1);
    // }

    // Close the backing store file
    fclose(backing_store_fp);

    // Update the page table entry for the required page
    set_page_table_entry(virtual_page_number, 1, 0, frame_number);

    // Update TLB
    add_tlb_entry(virtual_page_number, frame_number);
}




int translate_address(int virtual_address) {
    // Calculate virtual page number and offset
    int virtual_page_number = virtual_address / PAGE_SIZE;
    int offset = virtual_address % PAGE_SIZE;

    // Retrieve page table entry
    PageTableEntry *entry = get_page_table_entry(virtual_page_number);
    if (!entry) {
        printf("Error: Invalid virtual page number\n");
        return -1;
    }

    // Handle page fault if page is not valid
    if (!entry->valid_bit) {
        printf("Page fault! Virtual page: %d\n", virtual_page_number);
        page_faults++;
        handle_page_fault(virtual_page_number);
        // Retry translation after handling page fault
        entry = get_page_table_entry(virtual_page_number);
        if (!entry) {
            printf("Error: Page fault handling failed\n");
            return -1;
        }
    }

    // Calculate physical address using frame number from page table entry
    int physical_frame_number = entry->frame_number;
    int physical_address = physical_frame_number * PAGE_SIZE + offset;

    // Print for debugging (optional)
    printf("Virtual address: %d -> Physical address: %d\n", virtual_address, physical_address);

    return physical_address;
}


void testInput() {
    int address, offset;
    int page_idx, frame_idx;
    signed char data;
    char *ref, *out;
    FILE *fp = fopen("addresses.txt", "r");
    assert(fp);
    out = malloc(sizeof(char) * 6);
    while (!feof(fp)) {
        fscanf(fp, "%d", &address);
        /* first get the page offset and page number */
        offset = address % PAGE_SIZE;
        page_idx = (address / PAGE_SIZE) % NUM_PAGES;
        frame_idx = (page_table[page_idx]).frame_number;
        data = memory[frame_idx][offset];
        // printf("Virtual address: %d, Physical address: %d, Value: %d\n", address, (frame_idx)*FRAME_SIZE + offset, data);
    }

    // Print additional information in the console
    printf("Page numbers: %d, Page size: %d\n", NUM_PAGES, PAGE_SIZE);
    printf("Frame numbers: %d, Frame size: %d\n", NUM_FRAMES, FRAME_SIZE);
    printf("Page fault: %.3f%%\n", page_faults * 100.0 / 1000);
    printf("TLB hit: %.3f%%\n", tlb_hits * 100.0 / 1000);

    

   
    free(out);
}



// Main function for testing
int main() {
    initialize_page_table();
    initialize_tlb();

    memory = malloc(sizeof(char *) * NUM_FRAMES);
    for(int i=0;i<NUM_FRAMES;i++)   memory[i] = malloc(sizeof(char) * FRAME_SIZE);

    
    FILE *input_file = fopen("addresses.txt", "r");
    FILE *output_file = fopen("output.txt", "w");
    if (!input_file || !output_file) {
        fprintf(stderr, "Error opening files.\n");
        return 1;
    }

    // Read logical addresses from input file and translate them
    int logical_address;
    while (fscanf(input_file, "%d", &logical_address) == 1) {
        int physical_address = translate_address(logical_address);
        if (physical_address != -1) {
            fprintf(output_file, "%d\n", physical_address);
        } else {
            fprintf(output_file, "Page fault\n");
        }
    }

    // Close files
    fclose(input_file);
    fclose(output_file);

    testInput();

    return 0;
}
