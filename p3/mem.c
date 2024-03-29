////////////////////////////////////////////////////////////////////////////////
// Main File:        mem.c
// This File:        mem.c
// Other Files:      mem.h (not included in submission)
// Semester:         CS 354 Fall 2018
//
// Author:           David Zhu
// Email:            dzhu46@wisc.edu
// CS Login:         zhu
//
/////////////////////////// OTHER SOURCES OF HELP //////////////////////////////
//                   fully acknowledge and credit all sources of help,
//                   other than Instructors and TAs.
//
// Persons:          N/A
//
// Online sources:  StackExchange- Necessity of deleting footers when
//                                 coalescing
//                  StackOverflow- 8 byte alignment example
//                  TutorialsPoint- keeping track of free and allocated blocks
//
//////////////////////////// 80 columns wide ///////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/*
 * This structure serves as the header for each allocated and free block
 * It also serves as the footer for each free block
 * The blocks are ordered in the increasing order of addresses 
 */
typedef struct blk_hdr {                         
        int size_status;
  
    /*
    * Size of the block is always a multiple of 8
    * => last two bits are always zero - can be used to store other
    * information
    *
    * LSB -> Least Significant Bit (Last Bit)
    * SLB -> Second Last Bit 
    * LSB = 0 => free block
    * LSB = 1 => allocated/busy block
    * SLB = 0 => previous block is free
    * SLB = 1 => previous block is allocated/busy
    * 
    * When used as the footer the last two bits should be zero
    */

    /*
    * Examples:
    * 
    * For a busy block with a payload of 20 bytes (i.e. 20 bytes data + an
    * additional 4 bytes for header)
    * Header:
    * If the previous block is allocated, size_status should be set to 27
    * If the previous block is free, size_status should be set to 25
    * 
    * For a free block of size 24 bytes (including 4 bytes for header + 4
    * bytes for footer)
    * Header:
    * If the previous block is allocated, size_status should be set to 26
    * If the previous block is free, size_status should be set to 24
    * Footer:
    * size_status should be 24
    * 
    */
} blk_hdr;

/* Global variable - This will always point to the first block
 * i.e. the block with the lowest address */
blk_hdr *first_blk = NULL;

// Global variable - Total available memory
int total_mem_size = 0;

/*
 * Note: 
 *  The end of the available memory can be determined using end_mark
 *  The size_status of end_mark has a value of 1
 *
 */

/* 
 * Function for allocating 'size' bytes
 * Returns address of allocated block on success 
 * Returns NULL on failure 
 * Here is what this function should accomplish 
 * - Check for sanity of size - Return NULL when appropriate 
 * - Round up size to a multiple of 8 
 * - Traverse the list of blocks and allocate the best free block which can
 * accommodate the requested size
 * - Also, when allocating a block - split it into two blocks
 * Tips: Be careful with pointer arithmetic 
 */                    
void* Alloc_Mem(int size) {


    if (total_mem_size < 8) {
        return NULL;
    }

    // add padding to block
    int blockSize = size + 4;

    int padding = 0;
    if (blockSize % 8 != 0) {
        padding = 8 - (blockSize % 8);
    }

    blockSize += padding;
    blk_hdr* current = first_blk;
    blk_hdr* ptr = NULL;
    // t_begin will point to the header of the current block we're looking at
    char *t_begin = 0;

    // t_size will be the real size of the block we're looking at
    int t_size = 0;

    // smallest block will store the size of the smallest block we can fit our
    // requested block in for best fit

    int smallestBlock = 100000;
    while (current < (blk_hdr*) ((char*)first_blk + total_mem_size)) {

        t_begin = (char*)current;
        t_size = current->size_status - (current->size_status & 3);
        //check status of the block
        int status = current->size_status & 3;
        int currFree = 1;

        if (status % 2 == 0) {
            currFree = 0;
        }

        // looks for blocks that are free, smaller than our current smallest
        // free block, while being larger than the requested size allocation
        if (currFree == 0) {
            if (t_size > blockSize) {
                if (t_size < smallestBlock) {
                    smallestBlock = t_size;
                    ptr = (blk_hdr*)t_begin;
                }
            }
                // found correct sized block, break
            else if (t_size == blockSize) {
                ptr = (blk_hdr*)t_begin;
                break;
            }
        }
        // advance current pointer
        current = (blk_hdr*)((char*)current + t_size);
    }
    // if we iterate through list and ptr is still null, return NULL
    if (ptr == NULL) {
        return NULL;
    }

    // if the block we're about to allocate is large, we will split it only
    // if the remaining free block is larger than 8 bytes
    if (smallestBlock - blockSize < 8) {
        blockSize = smallestBlock;
        // check if block size 8-byte aligned
        if (blockSize % 8 != 0) {
            return NULL;
        }
    }
    // set size of the currently allocated block
    ptr->size_status = blockSize;
    // next block will point to the next block to set the next block's
    // previous status to 1
    blk_hdr* nextBlock = ptr;

    int currentBlockSize = ptr->size_status - (ptr->size_status & 3);
    // next block now points to the header of its block
    nextBlock = ((void*)((char*)ptr) + currentBlockSize);


    // diff is the size of the new free block that we split
    int diff = 0;
    if (smallestBlock - ptr->size_status >= 8 ) {
        diff = smallestBlock - ptr->size_status;
        nextBlock->size_status = diff;
    }
    blk_hdr* nextBlockFooter;

    // if the next block is free, set the footer of the free block
    if ((nextBlock->size_status & 1) == 0) {
        nextBlockFooter = nextBlock;
        nextBlockFooter = (void*)(((char*)nextBlockFooter) + 4);
        nextBlockFooter->size_status = diff;
        nextBlockFooter->size_status += 2;
    }

    //next block's previous block is busy
    nextBlock->size_status += 2;

    //set the current block is busy
    ptr->size_status += 1;

    //ptr now points to the payload of the allocated block
    ptr = (void*)((char*)ptr + 4);

    return ptr;
}

/* 
 * Function for freeing up a previously allocated block 
 * Argument - ptr: Address of the block to be freed up 
 * Returns 0 on success 
 * Returns -1 on failure 
 * Here is what this function should accomplish 
 * - Return -1 if ptr is NULL
 * - Return -1 if ptr is not 8 byte aligned or if the block is already freed
 * - Mark the block as free 
 * - Coalesce if one or both of the immediate neighbours are free 
 */                    
int Free_Mem(void *ptr) {                        

    // check if pointer is null
    if (ptr == NULL) {
        return -1;
    }

    blk_hdr* start = ptr - 4;
    // firstFree will point to the first free block.
    // if previous block is busy, point to current
    // if prev bloc free, point to prev block
    blk_hdr* firstFree = start;
    int ptrSize = start->size_status - (start->size_status & 3);
    char* lastAddress = (char*)first_blk + total_mem_size;

    // check if the ptr is out of bounds
    if (start < first_blk || (char*)start > lastAddress) {
        return -1;
    }

    // check if 8-byte aligned
    if (ptrSize % 8 != 0) {
        return -1;
    }

    // set the next block pointer
    blk_hdr* nextBlock = (blk_hdr*)((void*)((char*)start) + ptrSize);
    // if the next block is free, coalesce
    int nextBlockSize = 0;
    int prevPtrSize = 0;
    // footer pointer will point to the footer of the rightmost free block
    // that we coalesce
    blk_hdr* footerPtr = (blk_hdr*)(((void*)((char*)start) + ptrSize) - 4);
    //prevPtr will point to the previous block if its free
    blk_hdr* prevPtr;

    // checks the status of the next block
    int nextBlockStatus = nextBlock->size_status;
    int nextBlockFree = (nextBlockStatus & 1);

    // if the next block is within bounds, if its free, and if the status is
    // less than total_mem_size
    if ((char*)nextBlock < lastAddress && (nextBlockFree == 0)
        && nextBlockStatus < total_mem_size) {
        // if it is, store the real size of the next block
        nextBlockSize = nextBlock->size_status - (nextBlock->size_status & 3);
        // set footerPtr to the footer of the next block
        footerPtr = (((void*)((char*)nextBlock) + nextBlockSize) - 4);
    }

    // checks if the previous block of current is free or not
    if ((start->size_status & 2) == 0) {
        prevPtr = start;
        // prevptr now points at prev footer
        prevPtr = (blk_hdr*)((void*)((char*)prevPtr) - 4);
        prevPtrSize = prevPtr->size_status - (prevPtr->size_status & 3);
        // prev pointer takes start pointer and subtracts the prev
        // block's size
        prevPtr = (blk_hdr*)((void*)((char*)start) - prevPtrSize);
        firstFree = prevPtr;
    }

    // size of the free block that we coalesced
    int freeSize = ptrSize + nextBlockSize + prevPtrSize;
    // set the statuses of the header and footer of this free block
    footerPtr->size_status = freeSize;
    footerPtr->size_status += 2;

    firstFree->size_status = freeSize;
    firstFree->size_status += 2;

    return 0;
}

/*
 * Function used to initialize the memory allocator
 * Not intended to be called more than once by a program
 * Argument - sizeOfRegion: Specifies the size of the chunk which needs to
 * be allocated
 * Returns 0 on success and -1 on failure 
 */                    
int Init_Mem(int sizeOfRegion)
{                         
    int pagesize;
    int padsize;
    int fd;
    int alloc_size;
    void* space_ptr;
    blk_hdr* end_mark;
    static int allocated_once = 0;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: Init_Mem has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, 
                    fd, 0);
    if (MAP_FAILED == space_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
     allocated_once = 1;

    // for double word alignement and end mark
    alloc_size -= 8;

    // To begin with there is only one big free block
    // initialize heap so that first block meets 
    // double word alignement requirement
    first_blk = (blk_hdr*) space_ptr + 1;
    end_mark = (blk_hdr*)((void*)first_blk + alloc_size);
  
    // Setting up the header
    first_blk->size_status = alloc_size;

    // Marking the previous block as busy
    first_blk->size_status += 2;

    // Setting up the end mark and marking it as busy
    end_mark->size_status = 1;

    // Setting up the footer
    blk_hdr *footer = (blk_hdr*) ((char*)first_blk + alloc_size - 4);
    footer->size_status = alloc_size;
  
    return 0;
}

/* 
 * Function to be used for debugging 
 * Prints out a list of all the blocks along with the following information i
 * for each block 
 * No.      : serial number of the block 
 * Status   : free/busy 
 * Prev     : status of previous block free/busy
 * t_Begin  : address of the first byte in the block (this is where the header
 * starts)
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block (as stored in the block header) (including the
 * header/footer)
 */                     
void Dump_Mem() {                        
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    blk_hdr *current = first_blk;
    counter = 1;

    int busy_size = 0;
    int free_size = 0;
    int is_busy = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => busy block
            strcpy(status, "Busy");
            is_busy = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_busy = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "Busy");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_busy) 
            busy_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blk_hdr*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total busy size = %d\n", busy_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", busy_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;
}
