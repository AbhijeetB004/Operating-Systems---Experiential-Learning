#include<iostream>
#include<Windows.h>

#include "sm.h"



#define DO_POOL_ALLOCATION false

#define DO_DEALLOCATIONS_ALWAYS false   // Priority 1 - If this is true, value of DO_DEALLOCATIONS is ignored
#define DO_DEALLOCATIONS true           // Priority 2

#define MAX_ALLOCATIONS 0xffffff // 2^24  allocations || 0xffffff = 16777215
#define MAX_ALLOCATION_VALUE 128 // each allocation can have max size of 128

const unsigned int POOL_SIZE = 0xfffff; // poolsize = 1MB || 0xfffff = 1048575
const unsigned int INITIAL_POOLS[] = { 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128};

int main()
{
    if (DO_POOL_ALLOCATION)
    {
        
        printf("Allocation from POOL\n");
        int numPools = sizeof(INITIAL_POOLS) / sizeof(INITIAL_POOLS[0]);
        initStorageManager(POOL_SIZE, numPools, INITIAL_POOLS);

        //displayPoolInfo();
    }
    else
    {
        printf("Allocation from HEAP\n");
    }

    unsigned int i = 0;
    unsigned int sz = 0;
    vector<unsigned int>  allocSize;
    while (i < MAX_ALLOCATIONS)
    {
        do
        {// rand()% (MAX_ALLOCATION_VALUE -8 + 1)
            sz = 8 + rand()% (MAX_ALLOCATION_VALUE -8 + 1);
            //sz = 8 + g_rndGenerator.generateRandom(MAX_ALLOCATION_VALUE -8 + 1);
        } while (sz % 8 != 0);

        allocSize.push_back(sz);
        i++;
    }


    DWORD count = GetTickCount();
    unsigned int heapAllocs = 0;
    unsigned int heapFrees = 0;

    i = 0;
    while (i < MAX_ALLOCATIONS)
    {
        if (DO_POOL_ALLOCATION)
        {
            char *ptr = SM_ALLOC_ARRAY(char, allocSize[i]);
            if (ptr == nullptr)
            {
                printf("ERROR: ALLOC FAILED!\n");
                abort();
            }

            if (DO_DEALLOCATIONS_ALWAYS)
            {
                SM_DEALLOC(ptr);
            }
            else if (DO_DEALLOCATIONS && (rand()%10) < 5)
            {
                SM_DEALLOC(ptr);
            }
            else
            {
                //DON'T DEALLOCATE 
            }
        }
        else
        {
            char *ptr = (char *)malloc(allocSize[i]);
            if (ptr)
            {
                heapAllocs++;
            }
            else
            {
                printf("\n\n**HEAP ALLOCATION FAILURE!**\n");
                abort();
            }

            if (DO_DEALLOCATIONS_ALWAYS)
            {
                free(ptr);
                ptr == nullptr;
                heapFrees++;
            }
            else if (DO_DEALLOCATIONS && (rand()%10 )< 5)
            {
                free(ptr);
                ptr == nullptr;
                heapFrees++;
            }
            else
            {
                //DON'T DEALLOCATE 
            }
        }
        i++;
    }

    DWORD timetaken = GetTickCount() - count;

    if (DO_POOL_ALLOCATION)
    {
        displayPoolInfo();
    }
    else
    {
        printf("\n");
        printf("heapAllocs  : %u\n", heapAllocs);
        printf("heapFrees   : %u", heapFrees);
    }

    cout << "\n\n** Interval = " << timetaken << " ms ****" << endl;

    if (DO_POOL_ALLOCATION)
    {
        destroyStorageManager();
    }

    getchar(); 
    return 0;
}
