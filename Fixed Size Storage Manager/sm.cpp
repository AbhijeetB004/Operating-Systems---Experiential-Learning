#include "sm.h"
#include<map>
#include <string.h>

unsigned int m_initialPoolSize;              
//vector<int> m_PoolSizes;
map<unsigned int, PoolData_t*> m_PoolMap;    // Mapping of size and pool

void initStorageManager(const unsigned initialPoolSize, int numPools, const unsigned int *pools)
{
    m_initialPoolSize = initialPoolSize;
    unsigned int totalClaimedMemory = 0;

    printf("StorageManager:: Initial Pools- ");
    for (int i = 0; i < numPools; i++)
    {
        PoolData_t *poolData = (PoolData_t *)malloc(sizeof(PoolData_t));
        if (poolData == nullptr)
        {
            printf("\n\n**MEMORY ERROR: initStorageManager: Failed to create pool %u!!\n\n", pools[i]);
            abort();
        }

        memset(poolData, 0, sizeof(poolData));

        size_t sizeOfPoolInBytes = pools[i] * sizeof(char) * initialPoolSize;
        totalClaimedMemory += sizeOfPoolInBytes;
        char *ptr = (char *)malloc(sizeOfPoolInBytes);

        initializePoolData(sizeOfPoolInBytes, ptr, pools[i], poolData);

        printf("%d ", pools[i]);
    }

    
    printf("\nStorageManager:: Pool init complete\n");
    printf("Total claimed memory: %u MB\n\n", totalClaimedMemory/1000/1000);

}

void createNewPool(unsigned sizeId)
{
    PoolData_t *poolData = (PoolData_t *)malloc(sizeof(PoolData_t));
    if (poolData == nullptr)
    {
        printf("\n\n**MEMORY ERROR: createNewPool: Failed to create pool %u!!\n\n", sizeId);
        abort();
    }

    memset(poolData, 0, sizeof(poolData));

    size_t sizeOfPoolInBytes = sizeId * sizeof(char) * m_initialPoolSize;
    char *ptr = (char *)malloc(sizeOfPoolInBytes);

    initializePoolData(sizeOfPoolInBytes, ptr, sizeId, poolData);
    printf("Created new pool : %u\n", sizeId);
}

void initializePoolData(size_t sizeOfPoolInBytes, char *ptr, unsigned int sizeId, PoolData_t *poolData)
{
    poolData->poolSize = sizeId;
    poolData->startAddress = ptr;
    poolData->endAddress = ptr + sizeOfPoolInBytes;
    poolData->totalSize = sizeOfPoolInBytes;
    poolData->remainingSpace = sizeOfPoolInBytes;
    poolData->totalBlocks = (poolData->endAddress - poolData->startAddress) / poolData->poolSize;
    poolData->freeBlocks = poolData->totalBlocks;
    poolData->usedBlocks = 0;
    poolData->nextFreeBlock = -1;
    poolData->nextFreeBlockInSequence = 0;
    poolData->isFreedBlockBlockAvailable = false;
    poolData->totalAllocationsFromThisPool = 0;

    m_PoolMap[sizeId] = poolData;
}

void expandPool(unsigned size)
{
    //TODO
}

void displayPoolInfo()
{
    map<unsigned int, PoolData_t*>::iterator it = m_PoolMap.begin();
    
    printf("\n\n");
    while (it != m_PoolMap.end())
    {
        PoolData_t *poolData = it->second;
        unsigned int poolSize = it->first;

        
        printf("Pool %u\n", poolSize);
        
        printf("  totalAllocationsFromThisPool       : %u\n", poolData->totalAllocationsFromThisPool);
        printf("  startAddress                       : 0x%x\n", poolData->startAddress);
        printf("  endAddress                         : 0x%x\n", poolData->endAddress);
        printf("  totalSize                          : %u bytes\n", poolData->totalSize);
        printf("  remainingSpace                     : %u bytes\n", poolData->remainingSpace);
        printf("  totalBlocks                        : %u\n", poolData->totalBlocks);
        printf("  freeBlocks                         : %u\n", poolData->freeBlocks);
        printf("  usedBlocks                         : %u\n", poolData->usedBlocks);
        printf("\n");

        it++;
    }
    printf("\n** Total Pools: %d **\n", m_PoolMap.size());
}

void destroyStorageManager()
{
    //TODO
}

/**
 * The function `SM_alloc` allocates memory from a pool based on the requested size, either reusing
 * freed blocks or allocating new blocks.
 * 
 * @param size The `size` parameter in the `SM_alloc` function represents the size of memory to be
 * allocated in bytes. This function is responsible for allocating memory from a memory pool based on
 * the specified size. If a pool of the required size is not present, it creates a new pool. The
 * function then
 * 
 * @return The function `SM_alloc` is returning a pointer of type `void` which points to the allocated
 * memory block.
 */
void * SM_alloc(size_t size)
{
    //printf("SM_alloc called for %d bytes\n", size);

    /* Find which pool to use. If pool of required size not present, create a pool */
    PoolData_t *poolData = m_PoolMap[size];
    if (poolData != nullptr)
    {
        /* Check if this pool has enough free blocks */
        if (poolData->freeBlocks == 0)
        {
            printf("ERROR: Pool %u exhausted!\n", size);
            abort();
            //expandPool(size);
        }
    }
    else
    {
        createNewPool(size);
        poolData = m_PoolMap[size];
    }

    char *ptr = nullptr;

    if (poolData->isFreedBlockBlockAvailable && poolData->nextFreeBlock >= 0)
    {
        /* Allocating a block which was freed earlier. */
        ptr = findAddressFromBlock(poolData->nextFreeBlock, poolData);
        poolData->isFreedBlockBlockAvailable = false;
        poolData->nextFreeBlock = -1;
        //printf(">> From freed block, poolData->nextFreeBlock = %u\n", poolData->nextFreeBlock);
    }
    else
    {
        /* Allocating from free blocks in sequnce */
        ptr = findAddressFromBlock(poolData->nextFreeBlockInSequence, poolData);
        //printf(">> From sequence\n");
        poolData->nextFreeBlockInSequence++;
    }

    //printf("Allocated 0x%x (block %u) in pool %u\n", ptr, poolData->usedBlocks, poolData->poolSize);

    
    poolData->freeBlocks--;
    poolData->usedBlocks++;
    poolData->remainingSpace -= size;
    poolData->totalAllocationsFromThisPool++;

    return ptr;
}

void SM_dealloc(void *ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    /* Find the pool in which this address lies. */
    unsigned int poolSize = findPoolFromAddress(ptr);
    //printf("Deallocating 0x%x from pool %u\n", ptr, poolSize);

    /* Mark this address as free */
    PoolData_t *poolData = m_PoolMap[poolSize];
    poolData->freeBlocks++;
    poolData->usedBlocks--;
    poolData->nextFreeBlock = findBlockFromAddress((char *)ptr, poolData);
    //printf("Setting nextFreeBlock=%u in pool %u\n", poolData->nextFreeBlock, poolData->poolSize);
    poolData->isFreedBlockBlockAvailable = true;
    poolData->remainingSpace += poolSize;

    //printf("Deallocated 0x%x block (%u) from pool %u\n", ptr, poolData->nextFreeBlock, poolSize);
}

unsigned int findPoolFromAddress(void *ptr)
{
    map<unsigned int, PoolData_t*>::iterator it = m_PoolMap.begin();
    while (it != m_PoolMap.end())
    {
        PoolData_t *poolData = it->second;
        //printf(" Checking 0x%x in pool %u [0x%x, 0x%x] \n", ptr, poolData->poolSize, poolData->startAddress, poolData->endAddress);
        if (ptr >= poolData->startAddress && ptr <= poolData->endAddress)
        {
            return poolData->poolSize;
        }
        it++;
    }

    /* If we reached here, it means something is wrong! */
    printf("\n\n**ERROR: 0x%x not present in any of the memory pools!!\n\n", ptr);
    abort();
}

char *findAddressFromBlock(unsigned int block, PoolData_t *poolData)
{
    return poolData->startAddress + (block * poolData->poolSize);
}

unsigned int findBlockFromAddress(char *addr, PoolData_t *poolData)
{
    return (addr - poolData->startAddress) / poolData->poolSize;    
}
