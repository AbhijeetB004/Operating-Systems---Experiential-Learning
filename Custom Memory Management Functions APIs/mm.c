#include <stdio.h>
#include <memory.h>
#include <unistd.h>     /*for getpagesize*/
#include <sys/mman.h>   /*For using mmap()*/
#include <stdint.h>
#include "mm.h"
#include <assert.h>
#include <unistd.h> // For sleep function




static vm_page_for_families_t *first_vm_page_for_families = NULL;
static size_t SYSTEM_PAGE_SIZE = 0;

void
mm_init(){

    SYSTEM_PAGE_SIZE = getpagesize();
}

static inline uint32_t
mm_max_page_allocatable_memory(int units){

    return (uint32_t)
        ((SYSTEM_PAGE_SIZE * units) - offset_of(vm_page_t, page_memory));
}

#define MAX_PAGE_ALLOCATABLE_MEMORY(units) \
    (mm_max_page_allocatable_memory(units))


/*Function to request VM page from kernel*/
static void *
mm_get_new_vm_page_from_kernel(int units){

    char *vm_page = mmap(
        0,
        units * SYSTEM_PAGE_SIZE,
        PROT_READ|PROT_WRITE|PROT_EXEC,
        MAP_ANON|MAP_PRIVATE,
        0, 0);

    if(vm_page == MAP_FAILED){
        printf("Error : VM Page allocation Failed\n");
        return NULL;
    }
    memset(vm_page, 0, units * SYSTEM_PAGE_SIZE);
    return (void *)vm_page;
}

/*Function to return a page to kernel*/

static void
mm_return_vm_page_to_kernel (void *vm_page, int units){

    if(munmap(vm_page, units * SYSTEM_PAGE_SIZE)){
        printf("Error : Could not munmap VM page to kernel");
    }
}

static int
mm_get_hard_internal_memory_frag_size(
        block_meta_data_t *first,
        block_meta_data_t *second){

    block_meta_data_t *next_block = NEXT_META_BLOCK_BY_SIZE(first);
    return (int)((unsigned long)second - (unsigned long)(next_block));
}

static void
mm_union_free_blocks(block_meta_data_t *first,
        block_meta_data_t *second){

    assert(first->is_free == MM_TRUE &&
            second->is_free == MM_TRUE);

    first->block_size += sizeof(block_meta_data_t) +
        second->block_size;

    first->next_block = second->next_block;

    if(second->next_block)
        second->next_block->prev_block = first;
}

vm_page_t *
allocate_vm_page(vm_page_family_t *vm_page_family){

    vm_page_t *vm_page = mm_get_new_vm_page_from_kernel(1);
   
    /*Initialize lower most Meta block of the VM page*/
    MARK_VM_PAGE_EMPTY(vm_page);

    vm_page->block_meta_data.block_size =
        mm_max_page_allocatable_memory(1);
    vm_page->block_meta_data.offset =
        offset_of(vm_page_t, block_meta_data);
    init_glthread(&vm_page->block_meta_data.priority_thread_glue);
    vm_page->next = NULL;
    vm_page->prev = NULL;

    /*Set the back pointer to page family*/
    vm_page->pg_family = vm_page_family;

    /*If it is a first VM data page for a given
     * page family*/
    if(!vm_page_family->first_page){
        vm_page_family->first_page = vm_page;
        return vm_page;
    }

    /* Insert new VM page to the head of the linked 
     * list*/
    vm_page->next = vm_page_family->first_page;
    vm_page_family->first_page->prev = vm_page;
    vm_page_family->first_page = vm_page;
    return vm_page;
}

void
mm_vm_page_delete_and_free(
        vm_page_t *vm_page){

    vm_page_family_t *vm_page_family =
        vm_page->pg_family;

    /*If the page being deleted is the head of the linked 
     * list*/
    if(vm_page_family->first_page == vm_page){
        vm_page_family->first_page = vm_page->next;
        if(vm_page->next)
            vm_page->next->prev = NULL;
        vm_page->next = NULL;
        vm_page->prev = NULL;
        mm_return_vm_page_to_kernel((void *)vm_page, 1);
        return;
    }

    /*If we are deleting the page from middle or end of 
     * linked list*/
    if(vm_page->next)
        vm_page->next->prev = vm_page->prev;
    vm_page->prev->next = vm_page->next;
    mm_return_vm_page_to_kernel((void *)vm_page, 1);
}

void
mm_print_vm_page_details(vm_page_t *vm_page){

    printf("\t\t next = %p, prev = %p\n", vm_page->next, vm_page->prev);
    printf("\t\t page family = %s\n", vm_page->pg_family->struct_name);

    uint32_t j = 0;
    block_meta_data_t *curr;
    ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page, curr){

        printf("\t\t\t%-14p Block %-3u %s  block_size = %-6u  "
                "offset = %-6u  prev = %-14p  next = %p\n",
                curr,
                j++, curr->is_free ? "F R E E D" : "ALLOCATED",
                curr->block_size, curr->offset,
                curr->prev_block,
                curr->next_block);
    } ITERATE_VM_PAGE_ALL_BLOCKS_END(vm_page, curr);
}


void
mm_instantiate_new_page_family(
    char *struct_name,
    uint32_t struct_size){


    vm_page_family_t *vm_page_family_curr = NULL;
    vm_page_for_families_t *new_vm_page_for_families = NULL;

    if(struct_size > SYSTEM_PAGE_SIZE){
        
        printf("Error : %s() Structure %s Size exceeds system page size\n",
            __FUNCTION__, struct_name);
        return;
    }

    if(!first_vm_page_for_families){

        first_vm_page_for_families = 
            (vm_page_for_families_t *)mm_get_new_vm_page_from_kernel(1);
        first_vm_page_for_families->next = NULL;
        strncpy(first_vm_page_for_families->vm_page_family[0].struct_name, 
        struct_name, MM_MAX_STRUCT_NAME);
        first_vm_page_for_families->vm_page_family[0].struct_size = struct_size;
        first_vm_page_for_families->vm_page_family[0].first_page = NULL;
        init_glthread(&first_vm_page_for_families->vm_page_family[0].free_block_priority_list_head);
        return;
    }

	vm_page_family_curr = lookup_page_family_by_name(struct_name);

	if(vm_page_family_curr) {
		assert(0);
	}

    uint32_t count = 0;

    ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families, vm_page_family_curr){

	    count++;

    } ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families, vm_page_family_curr);

    if(count == MAX_FAMILIES_PER_VM_PAGE){

        new_vm_page_for_families = 
            (vm_page_for_families_t *)mm_get_new_vm_page_from_kernel(1);
        new_vm_page_for_families->next = first_vm_page_for_families;
        first_vm_page_for_families = new_vm_page_for_families;
    }

    strncpy(vm_page_family_curr->struct_name, struct_name,
            MM_MAX_STRUCT_NAME);
    vm_page_family_curr->struct_size = struct_size;
    vm_page_family_curr->first_page = NULL;
    init_glthread(&vm_page_family_curr->free_block_priority_list_head);
}

void
mm_print_registered_page_families(){

    vm_page_family_t *vm_page_family_curr = NULL;
    vm_page_for_families_t *vm_page_for_families_curr = NULL;

    for(vm_page_for_families_curr = first_vm_page_for_families;
            vm_page_for_families_curr;
            vm_page_for_families_curr = vm_page_for_families_curr->next){

        ITERATE_PAGE_FAMILIES_BEGIN(vm_page_for_families_curr,
                vm_page_family_curr){

            printf("Page Family : %s, Size = %u\n",
                    vm_page_family_curr->struct_name,
                    vm_page_family_curr->struct_size);

        } ITERATE_PAGE_FAMILIES_END(vm_page_for_families_curr,
                vm_page_family_curr);
    }
}

static int
free_blocks_comparison_function(
        void *_block_meta_data1,
        void *_block_meta_data2){

    block_meta_data_t *block_meta_data1 =
        (block_meta_data_t *)_block_meta_data1;

    block_meta_data_t *block_meta_data2 =
        (block_meta_data_t *)_block_meta_data2;

    if(block_meta_data1->block_size > block_meta_data2->block_size)
        return -1;
    else if(block_meta_data1->block_size < block_meta_data2->block_size)
        return 1;
    return 0;
}

static void
mm_add_free_block_meta_data_to_free_block_list(
        vm_page_family_t *vm_page_family,
        block_meta_data_t *free_block){

    assert(free_block->is_free == MM_TRUE);
    glthread_priority_insert(&vm_page_family->free_block_priority_list_head,
            &free_block->priority_thread_glue,
            free_blocks_comparison_function,
            offset_of(block_meta_data_t, priority_thread_glue));
}

static vm_page_t *
mm_family_new_page_add(vm_page_family_t *vm_page_family){

    vm_page_t *vm_page = allocate_vm_page(vm_page_family);

    if(!vm_page)
        return NULL;

    /* The new page is like one free block, add it to the
     * free block list*/
    mm_add_free_block_meta_data_to_free_block_list(
            vm_page_family, &vm_page->block_meta_data);

    return vm_page;
}

/* Fn to mark block_meta_data as being Allocated for
 * 'size' bytes of application data. Return TRUE if 
 * block allocation succeeds*/
static vm_bool_t
mm_split_free_data_block_for_allocation(
            vm_page_family_t *vm_page_family,
            block_meta_data_t *block_meta_data, 
            uint32_t size){

    block_meta_data_t *next_block_meta_data = NULL;

    assert(block_meta_data->is_free == MM_TRUE);

    if(block_meta_data->block_size < size){
        return MM_FALSE;
    }

    uint32_t remaining_size =
        block_meta_data->block_size - size;

    block_meta_data->is_free = MM_FALSE;
    block_meta_data->block_size = size;
    remove_glthread(&block_meta_data->priority_thread_glue);
    /*block_meta_data->offset =  ??*/

    /*Case 1 : No Split*/
    if(!remaining_size){
        return MM_TRUE;
    }

    /*Case 3 : Partial Split : Soft Internal Fragmentation*/
    else if(sizeof(block_meta_data_t) < remaining_size &&
            remaining_size < (sizeof(block_meta_data_t) + vm_page_family->struct_size)){
        /*New Meta block is to be created*/
        next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
        next_block_meta_data->is_free = MM_TRUE;
        next_block_meta_data->block_size =
            remaining_size - sizeof(block_meta_data_t);
        next_block_meta_data->offset = block_meta_data->offset +
            sizeof(block_meta_data_t) + block_meta_data->block_size;
        init_glthread(&next_block_meta_data->priority_thread_glue);
        mm_add_free_block_meta_data_to_free_block_list(
                vm_page_family, next_block_meta_data);
        mm_bind_blocks_for_allocation(block_meta_data, next_block_meta_data);
    }
   
    /*Case 3 : Partial Split : Hard Internal Fragmentation*/
    else if(remaining_size < sizeof(block_meta_data_t)){
        /*No need to do anything !!*/
    }

    /*Case 2 : Full Split  : New Meta block is Created*/
    else {
        /*New Meta block is to be created*/
        next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
        next_block_meta_data->is_free = MM_TRUE;
        next_block_meta_data->block_size =
            remaining_size - sizeof(block_meta_data_t);
        next_block_meta_data->offset = block_meta_data->offset +
            sizeof(block_meta_data_t) + block_meta_data->block_size;
        init_glthread(&next_block_meta_data->priority_thread_glue);
        mm_add_free_block_meta_data_to_free_block_list(
                vm_page_family, next_block_meta_data);
        mm_bind_blocks_for_allocation(block_meta_data, next_block_meta_data);
    }

    return MM_TRUE;

}



static block_meta_data_t *
mm_allocate_free_data_block(
        vm_page_family_t *vm_page_family,
        uint32_t req_size){
    
    vm_bool_t status = MM_FALSE;
    vm_page_t *vm_page = NULL;
    block_meta_data_t *block_meta_data = NULL;

    block_meta_data_t *biggest_block_meta_data =
        mm_get_biggest_free_block_page_family(vm_page_family);

    if(!biggest_block_meta_data ||
            biggest_block_meta_data->block_size < req_size){

        /*Time to add a new page to Page family to satisfy the request*/
        vm_page = mm_family_new_page_add(vm_page_family);

        /*Allocate the free block from this page now*/
        status = mm_split_free_data_block_for_allocation(vm_page_family,
                &vm_page->block_meta_data, req_size);

        if(status)
            return &vm_page->block_meta_data;

        return NULL;
    }
    /*The biggest block meta data can satisfy the request*/
    if(biggest_block_meta_data){
        status = mm_split_free_data_block_for_allocation(vm_page_family,
                biggest_block_meta_data, req_size);
    }

    if(status)
        return biggest_block_meta_data;

    return NULL;
}

vm_page_family_t *
lookup_page_family_by_name(char *struct_name){

    vm_page_family_t *vm_page_family_curr = NULL;
    vm_page_for_families_t *vm_page_for_families_curr = NULL;

    for(vm_page_for_families_curr = first_vm_page_for_families;
            vm_page_for_families_curr;
            vm_page_for_families_curr = vm_page_for_families_curr->next){

        ITERATE_PAGE_FAMILIES_BEGIN(vm_page_for_families_curr, vm_page_family_curr){

            if(strncmp(vm_page_family_curr->struct_name,
                        struct_name,
                        MM_MAX_STRUCT_NAME) == 0){

                return vm_page_family_curr;
            }
        } ITERATE_PAGE_FAMILIES_END(vm_page_for_families_curr, vm_page_family_curr);
    }
    return NULL;
}


/* The public fn to be invoked by the application for Dynamic
 * Memory Allocations.*/
/**
 * The `xcalloc` function allocates memory for a specified structure type and initializes it to zero.
 * 
 * @param struct_name The `struct_name` parameter in the `xcalloc` function is a pointer to a character
 * array that represents the name of a structure for which memory needs to be allocated.
 * @param units The `units` parameter in the `xcalloc` function represents the number of elements of a
 * specific size that you want to allocate memory for. It is used to calculate the total memory
 * required based on the size of the structure specified by `struct_name`.
 * 
 * @return The function `xcalloc` is returning a pointer to the allocated memory block. If the memory
 * allocation is successful, it returns a pointer to the start of the allocated memory block. If there
 * is an error during the allocation process, such as the structure not being registered with the
 * Memory Manager or the memory request exceeding the page size, it returns NULL.
 */
void *
xcalloc(char *struct_name, int units){

    /*Step 1*/  
     vm_page_family_t *pg_family =
             lookup_page_family_by_name(struct_name);

     if(!pg_family){

         printf("Error : Structure %s not registered with Memory Manager\n",
                 struct_name);
         return NULL;
     }

     if(units * pg_family->struct_size > MAX_PAGE_ALLOCATABLE_MEMORY(1)){

         printf("Error : Memory Requested Exceeds Page Size\n");
         return NULL;
     }
     
     /*Find the page which can satisfy the request*/
     block_meta_data_t *free_block_meta_data = NULL;

     free_block_meta_data = mm_allocate_free_data_block(
             pg_family, units * pg_family->struct_size);

     if(free_block_meta_data){
         memset((char *)(free_block_meta_data + 1), 0, 
         free_block_meta_data->block_size);
         return  (void *)(free_block_meta_data + 1);
     }

     return NULL;
}

static block_meta_data_t *
mm_free_blocks(block_meta_data_t *to_be_free_block){

    block_meta_data_t *return_block = NULL;

    assert(to_be_free_block->is_free == MM_FALSE);

    vm_page_t *hosting_page =
        MM_GET_PAGE_FROM_META_BLOCK(to_be_free_block);

    vm_page_family_t *vm_page_family = hosting_page->pg_family;

    return_block = to_be_free_block;

    to_be_free_block->is_free = MM_TRUE;

    block_meta_data_t *next_block = NEXT_META_BLOCK(to_be_free_block);

    /*Handling Hard IF memory*/
    if(next_block){
        /* Scenario 1 : When data block to be freed is not the last
         * upper most meta block in a VM data page*/
        to_be_free_block->block_size +=
            mm_get_hard_internal_memory_frag_size (to_be_free_block, next_block);
    }
    else {
        /* Scenario 2: Page Boundry condition*/
        /* Block being freed is the upper most free data block
         * in a VM data page, check of hard internal fragmented
         * memory and merge*/
        char *end_address_of_vm_page = (char *)((char *)hosting_page + SYSTEM_PAGE_SIZE);
        char *end_address_of_free_data_block =
            (char *)(to_be_free_block + 1) + to_be_free_block->block_size;
        int internal_mem_fragmentation = (int)((unsigned long)end_address_of_vm_page -
                (unsigned long)end_address_of_free_data_block);
        to_be_free_block->block_size += internal_mem_fragmentation;
    }

    /*Now perform Merging*/
    if(next_block && next_block->is_free == MM_TRUE){
        /*Union two free blocks*/
        mm_union_free_blocks(to_be_free_block, next_block);
        return_block = to_be_free_block;
    }
    /*Check the previous block if it was free*/
    block_meta_data_t *prev_block = PREV_META_BLOCK(to_be_free_block);

    if(prev_block && prev_block->is_free){
        mm_union_free_blocks(prev_block, to_be_free_block);
        return_block = prev_block;
    }

    if(mm_is_vm_page_empty(hosting_page)){
        mm_vm_page_delete_and_free(hosting_page);
        return NULL;
    }
    mm_add_free_block_meta_data_to_free_block_list(
            hosting_page->pg_family, return_block);

    return return_block;
}



/**
 * The function `xfree` frees a memory block and performs additional cleanup operations.
 * 
 * @param app_data The `app_data` parameter is a pointer to the memory block that was previously
 * allocated and needs to be freed.
 */
void
xfree(void *app_data){

    block_meta_data_t *block_meta_data =
        (block_meta_data_t *)((char *)app_data - sizeof(block_meta_data_t));

    assert(block_meta_data->is_free == MM_FALSE);
    mm_free_blocks(block_meta_data);
}

vm_bool_t
mm_is_vm_page_empty(vm_page_t *vm_page){

    if(vm_page->block_meta_data.next_block == NULL &&
            vm_page->block_meta_data.prev_block == NULL &&
            vm_page->block_meta_data.is_free == MM_TRUE){

        return MM_TRUE;
    }
    return MM_FALSE;
}


void mm_print_block_usage() {
    vm_page_t *vm_page_curr;
    vm_page_family_t *vm_page_family_curr;
    block_meta_data_t *block_meta_data_curr;
    uint32_t total_block_count, free_block_count,
             occupied_block_count;
    uint32_t application_memory_usage;

    // Color codes for different text elements
    char *color_block_usage = "\x1b[1m\x1b[95m"; // Bright magenta
    char *color_reset = "\x1b[0m"; // Reset color

    ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families, vm_page_family_curr) {
        total_block_count = 0;
        free_block_count = 0;
        application_memory_usage = 0;
        occupied_block_count = 0;

        ITERATE_VM_PAGE_BEGIN(vm_page_family_curr, vm_page_curr) {
            ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page_curr, block_meta_data_curr) {
                total_block_count++;

                // Sanity Checks
                if(block_meta_data_curr->is_free == MM_FALSE) {
                    assert(IS_GLTHREAD_LIST_EMPTY(&block_meta_data_curr->priority_thread_glue));
                }
                if(block_meta_data_curr->is_free == MM_TRUE) {
                    assert(!IS_GLTHREAD_LIST_EMPTY(&block_meta_data_curr->priority_thread_glue));
                }

                if(block_meta_data_curr->is_free == MM_TRUE) {
                    free_block_count++;
                } else {
                    application_memory_usage +=
                        block_meta_data_curr->block_size + sizeof(block_meta_data_t);
                    occupied_block_count++;
                }
            } ITERATE_VM_PAGE_ALL_BLOCKS_END(vm_page_curr, block_meta_data_curr);
        } ITERATE_VM_PAGE_END(vm_page_family_curr, vm_page_curr);

        // Output block usage information with improved formatting and color highlighting
        printf("%s%-20s   Total Block Count: %-4u    Free Block Count: %-4u    Occupied Block Count: %-4u    AppMemUsage: %u%s\n",
                color_block_usage, vm_page_family_curr->struct_name, total_block_count,
                free_block_count, occupied_block_count, application_memory_usage, color_reset);
    } ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families, vm_page_family_curr);
}

void mm_print_memory_usage(char *struct_name) {
    uint32_t i = 0;
    vm_page_t *vm_page = NULL;
    vm_page_family_t *vm_page_family_curr;
    uint32_t number_of_struct_families = 0;
    uint32_t cumulative_vm_pages_claimed_from_kernel = 0;

    // Color codes for different text elements
    char *color_summary = "\x1b[1m\x1b[94m"; // Bright blue
    char *color_struct = "\x1b[1m\x1b[92m"; // Bright green
    char *color_loading = "\x1b[1m\x1b[96m"; // Bright cyan
    char *color_block_usage = "\x1b[1m\x1b[95m"; // Bright magenta
    char *color_reset = "\x1b[0m"; // Reset color

    printf("\n%s========================================================================================================================================================================%s\n", color_summary, color_reset);
    printf("%s                             Memory Usage Summary                           %s\n", color_summary, color_reset);
    printf("%s=============================================================================================================================================================================%s\n\n", color_summary, color_reset);

    ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families, vm_page_family_curr) {
        if (struct_name && strncmp(struct_name, vm_page_family_curr->struct_name,
                                   strlen(vm_page_family_curr->struct_name))) {
            continue;
        }

        number_of_struct_families++;

        printf("%sStructure Family: %s%s\n", color_struct, vm_page_family_curr->struct_name, color_reset);
        printf("-------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        printf("%s%-15s | %-25s | %-20s%s\n", color_struct, "Page", "Usage", "Struct Size", color_reset);

        i = 0;

        ITERATE_VM_PAGE_BEGIN(vm_page_family_curr, vm_page) {
            cumulative_vm_pages_claimed_from_kernel++;
            mm_print_vm_page_details(vm_page);
        } ITERATE_VM_PAGE_END(vm_page_family_curr, vm_page);

        printf("\n");
        usleep(1000000); // Delay for 1 second
    } ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families, vm_page_family_curr);

    printf("\n%s==================================================================================================================================================================%s\n", color_summary, color_reset);
    printf("%sTotal VM Pages in Use: %-12u%s\n", color_summary, cumulative_vm_pages_claimed_from_kernel, color_reset);
    printf("%sTotal Memory Used: %-18lu Bytes%s\n", color_summary, SYSTEM_PAGE_SIZE * cumulative_vm_pages_claimed_from_kernel, color_reset);
    printf("%s==================================================================================================================================================================%s\n\n", color_summary, color_reset);

    // Simulate rotating loading effect
    char loading_chars[] = {'|', '/', '-', '\\'};
    int num_loading_chars = sizeof(loading_chars) / sizeof(loading_chars[0]);
    int iterations = 20; // Adjust the number of iterations to control the duration of the animation

    for (int j = 0; j < iterations; j++) {
        printf("%sLoading %c%s\r", color_loading, loading_chars[j % num_loading_chars], color_reset);
        fflush(stdout); // Flush the output buffer
        usleep(100000); // Sleep for 0.1 seconds
    }
    printf("%sLoading Complete!     %s\n", color_loading, color_reset); // Print completion message

    // Print block usage
    printf("\n%sShowing the block usage%s\n", color_block_usage, color_reset);

}

