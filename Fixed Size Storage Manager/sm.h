#ifndef SM_H
#define SM_H
#include<vector>
#include<stack>

using namespace std;

/* These macros should be used to allocate memory */
#define SM_ALLOC_ARRAY(type, size)      (type *)SM_alloc(size * sizeof(type))
#define SM_ALLOC(type)                  (type *)SM_alloc(sizeof(type))
#define SM_DEALLOC(ptr)                 SM_dealloc(ptr)

typedef struct PoolData_tag
{
    unsigned int poolSize;                       // Size of this pool
    char *startAddress;                          // Starting address of this pool
    char *endAddress;                            // Ending address of this pool
    unsigned int totalSize;                      // Total size of this pool
    unsigned int remainingSpace;                 // Remaining size left in this pool
    unsigned int totalBlocks;                    // Total blocks in this pool
    unsigned int freeBlocks;                     // Free blocks in this pool
    unsigned int usedBlocks;                     // Used blocks in this pool
    unsigned int nextFreeBlockInSequence;        // Next free block in sequence. This is always in order. 
    int nextFreeBlock;                          // Next free block available in this pool. This will be allocated 
                                                // to next alloc request. This can be in between the pool. This can
                                                // be -ve. If -ve, then memory needs to be allocated from nextFreeBlockInSequence.
    bool isFreedBlockBlockAvailable;
    stack<unsigned int> freeBlockStack;   // Stack storing block ID of free blocks
    unsigned int totalAllocationsFromThisPool;

}PoolData_t;

void initStorageManager(const unsigned int poolSize, int numPools, const unsigned int *pools);
void initializePoolData(size_t sizeToAllocate, char *ptr, unsigned int size, PoolData_t *poolData);
void displayPoolInfo();
void destroyStorageManager();
void *SM_alloc(size_t size);
void SM_dealloc(void *ptr);
unsigned int findPoolFromAddress(void *ptr);
char *findAddressFromBlock(unsigned int block, PoolData_t *poolData);
unsigned int findBlockFromAddress(char *addr, PoolData_t *poolData);
void createNewPool(unsigned size);
void expandPool(unsigned size);
#endif