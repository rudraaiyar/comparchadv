#include "tips.h"

/* The following two functions are defined in util.c */

/* finds the highest 1 bit, and returns its position, else 0xFFFFFFFF */
unsigned int uint_log2(word w);

/* return random int from 0..x-1 */
int randomint( int x );

/*
 This function allows the lfu information to be displayed
 
 assoc_index - the cache unit that contains the block to be modified
 block_index - the index of the block to be modified
 
 returns a string representation of the lfu information
 */
char* lfu_to_string(int assoc_index, int block_index)
{
    /* Buffer to print lfu information -- increase size as needed. */
    static char buffer[9];
    sprintf(buffer, "%u", cache[assoc_index].block[block_index].accessCount);
    
    return buffer;
}

/*
 This function allows the lru information to be displayed
 
 assoc_index - the cache unit that contains the block to be modified
 block_index - the index of the block to be modified
 
 returns a string representation of the lru information
 */
char* lru_to_string(int assoc_index, int block_index)
{
    /* Buffer to print lru information -- increase size as needed. */
    static char buffer[9];
    sprintf(buffer, "%u", cache[assoc_index].block[block_index].lru.value);
    
    return buffer;
}

/*
 This function initializes the lfu information
 
 assoc_index - the cache unit that contains the block to be modified
 block_number - the index of the block to be modified
 
 */
void init_lfu(int assoc_index, int block_index)
{
    cache[assoc_index].block[block_index].accessCount = 0;
}

/*
 This function initializes the lru information
 
 assoc_index - the cache unit that contains the block to be modified
 block_number - the index of the block to be modified
 
 */
void init_lru(int assoc_index, int block_index)
{
    cache[assoc_index].block[block_index].lru.value = 0;
}

/*
 This is the primary function you are filling out,
 You are free to add helper functions if you need them
 
 @param addr 32-bit byte address
 @param data a pointer to a SINGLE word (32-bits of data)
 @param we   if we == READ, then data used to return
 information back to CPU
 
 if we == WRITE, then data used to
 update Cache/DRAM
 */
void accessMemory(address addr, word* data, WriteEnable we)
{
    /* Declare variables here */
    
    unsigned int indexlen = 0, offsetlen = 0, taglen = 0;
    //index length = log2(# of sets)
    indexlen= uint_log2(set_count);
    //offset length = log2(block size in bytes)
    offsetlen= uint_log2(block_size);
    //tag length= 32- (offsetlen + indexlen)
    taglen= 32 - (offsetlen + indexlen);
    
    //calculate index value
    unsigned int indexval = addr << taglen;
    indexval = indexval >> (offsetlen + taglen);
    //calculate tag value
    unsigned int tagval = addr >> (offsetlen + indexlen);
    //calculate offset value
    unsigned int offsetval = addr << (indexlen + taglen);
    offsetval = offsetval >> (indexlen + taglen);
    
    address myAddr; //address instance
    
    TransferUnit byte_size;
    // blocksize 2^n from 2 to 5
    if (block_size ==4) {
        byte_size =2;
    }else if(block_size ==8){
        byte_size=3;
    }else if(block_size ==16){
        byte_size=4;
    }else if(block_size ==32){
        byte_size=5;
    }
    
    unsigned int blockHit = 0, policyIndex = 0, policyValue = 0;
    
    
    /* handle the case of no cache at all - leave this in */
    if(assoc == 0) {
        accessDRAM(addr, (byte*)data, WORD_SIZE, we);
        return;
    }
    
    /*
     You need to read/write between memory (via the accessDRAM() function) and
     the cache (via the cache[] global structure defined in tips.h)
     
     Remember to read tips.h for all the global variables that tell you the
     cache parameters
     
     The same code should handle random, LFU, and LRU policies. Test the policy
     variable (see tips.h) to decide which policy to execute. The LRU policy
     should be written such that no two blocks (when their valid bit is VALID)
     will ever be a candidate for replacement. In the case of a tie in the
     least number of accesses for LFU, you use the LRU information to determine
     which block to replace.
     
     Your cache should be able to support write-through mode (any writes to
     the cache get immediately copied to main memory also) and write-back mode
     (and writes to the cache only gets copied to main memory when the block
     is kicked out of the cache.
     
     Also, cache should do allocate-on-write. This means, a write operation
     will bring in an entire block if the block is not already in the cache.
     
     To properly work with the GUI, the code needs to tell the GUI code
     when to redraw and when to flash things. Descriptions of the animation
     functions can be found in tips.h
     */
    
    /* Start adding code here */
    
    //replacement policy LRU and Random
    //memory sync policy write back and write through
    //https://cseweb.ucsd.edu/classes/sp09/cse141/Slides/10_Caches_detail.pdf
    /***************************
     dirty - valid - tag - data
     ***************************/
    /* handling a miss
     (1) Send the address & read operation to the next level of the hierarchy
     (2) Wait for the data to arrive
     (3) Update the cache entry with data*, rewrite the tag, turn the valid bit on, clear
     the dirty bit (if data cache & write back)
     (4) Resend the memory address; this time there will be a hit.
     */
    
    /* dirty bit scenario ---email Daniel:
     Dirty bit is only used during write back, since the contents in the cache maybe different than the blocks in memory. Once a read miss occures, check if the block being replaced is dirty, if it is, copy the block back to memory before replacing it. Once replaced, reset the dirty bit. You don't need it for write through, as any updates in cache will be updated in memory as well.
     */
    /*
     {tag, index, offset} = address;
     if (isRead) {
     if (tags[index] == tag) {
     return data[index];
     } else {
     l = chooseLine(...);
     if (l is dirty) {
     WriteBack(l);
     }
     Load address into line l;
     return data[l];
     }
     }
     */
    
    if (we == READ){ //read case
        // no need to access RAM
        //hit case
        blockHit = 0;
        int i = 0; //init
        while( i < assoc){ //keep this going as long as we keep getting hits
            if (tagval == cache[indexval].block[i].tag == 1){ //tag = 1
                if (cache[indexval].block[i].valid == 1){// and is valid =1
                    /*
                     hit =yes
                     lru value =0
                     valid =1
                     accesscount = inc
                     */
                    blockHit = 1;
                    cache[indexval].block[i].lru.value = 0;
                    cache[indexval].block[i].valid = 1;
                    memcpy (data,(cache[indexval].block[i].data + offsetval), 4); //save all this
                    
                    highlight_block(indexval, i);
                    highlight_offset(indexval, i, offsetval, HIT);
                }
            }
            i++; //increment
        }
        if (blockHit == 0){ //miss case
            
            
            if (policy == RANDOM){ //random case which just sets index to any random number within assoc
                policyIndex = randomint(assoc); //set index using random function given
                //printf("lru index with assoc %d", policyIndex);
            }
            else if (policy == LRU){
                //http://www.mathcs.emory.edu/~cheung/Courses/355/Syllabus/9-virtual-mem/LRU-replace.html
                //Add a register to every page frame - contain the last time that the page in that frame was accessed
                //Use a "logical clock" that advance by 1 tick each time a memory reference is made.
                //Each time a page is referenced, update its register
                int i = 0; //init loop
                while( i < assoc){
                    if(policyValue < cache[indexval].block[i].lru.value){ // prev value is less than potential current
                        policyIndex = i; //set to current iteration
                        policyValue = cache[indexval].block[i].lru.value; //update
                        
                    }
                    i++;//increment
                }
            }
            
            //dirty case
            if(cache[indexval].block[policyIndex].dirty == DIRTY){ //if the bit is dirty
                myAddr = cache[indexval].block[policyIndex].tag << ((indexlen + offsetlen) + (indexval <<offsetlen)); //assist copy
                accessDRAM(myAddr, (cache[indexval].block[policyIndex].data), byte_size, WRITE); // copy this data and write to mem
            }
            
            //https://courses.cs.washington.edu/courses/csep548/06au/lectures/cacheAdv.pdf
            /* UPDATE CACHE ENTRY WITH DATA!!!!!
             hit = no, but next round yes
             lru value =0
             turn on valid =1
             clear dirty = virgin
             rewrite tag =value
             */
            
            accessDRAM(addr, (cache[indexval].block[policyIndex].data), byte_size, READ);
            cache[indexval].block[policyIndex].lru.value = 0;
            cache[indexval].block[policyIndex].valid = 1;
            cache[indexval].block[policyIndex].dirty = VIRGIN;
            cache[indexval].block[policyIndex].tag = tagval;
            memcpy (data,(cache[indexval].block[policyIndex].data + offsetval), 4); //save all this
            
            highlight_block(indexval, i- assoc);
            highlight_offset(indexval, i - assoc, offsetval, MISS);
        }
    }
    
    /*
     {tag, index, offset} = address;
     if (isWrite) {
     if (tags[index] == tag) {
     data[index] = data; // Should we just update locally?
     dirty[index] = true;
     } else {
     l = chooseLine(...); // maybe no line?
     if (l is dirty) {
     WriteBack(l);
     }
     if (l exists) {
     data[l] = data;
     }
     }
     }
     */
    
    else if (we == WRITE){ //WRITE CASE
        blockHit = 0;
        if (memory_sync_policy == WRITE_BACK) //write back has a dirty case
        {
            int i =0; //hit
            while (i < assoc)
            {
                if (tagval == cache[indexval].block[i].tag ==1){
                    if(cache[indexval].block[i].valid == 1){ // a block hit
                        blockHit = 1;
                        cache[indexval].block[i].lru.value = 0;
                        cache[indexval].block[i].valid = 1;
                        cache[indexval].block[i].dirty = DIRTY;
                        memcpy ((cache[indexval].block[i].data + offsetval),data, 4);
                        
                        highlight_block(indexval, i);
                        highlight_offset(indexval, i, offsetval, HIT);
                    }
                }
                i++;
            }
            if (blockHit == 0) // miss SAME AS READ?
            {
                
                
                if (policy == RANDOM){ //random case which just sets index to any random number within assoc
                    policyIndex = randomint(assoc); //set index using random function given
                    
                }
                else if (policy == LRU){
                    
                    int i = 0; //init loop
                    while( i < assoc){
                        if(policyValue < cache[indexval].block[i].lru.value){ // prev value is less than potential current
                            policyIndex = i; //set to current iteration
                            policyValue = cache[indexval].block[i].lru.value; //update
                            
                        }
                        i++;//increment
                    }
                }
                //dirty case
                if(cache[indexval].block[policyIndex].dirty == DIRTY){ //if the bit is dirty
                    myAddr = cache[indexval].block[policyIndex].tag << ((indexlen + offsetlen) + (indexval <<offsetlen)); //assist copy
                    accessDRAM(myAddr, (cache[indexval].block[policyIndex].data), byte_size, WRITE); // copy this data and write to mem
                }
                
                highlight_block(indexval, i-2);
                highlight_offset(indexval, i-2, offsetval, MISS);
                
                cache[indexval].block[policyIndex].lru.value = 0;
                cache[indexval].block[policyIndex].valid = 1;
                cache[indexval].block[policyIndex].dirty = VIRGIN;
                cache[indexval].block[policyIndex].tag = tagval;
                memcpy ((cache[indexval].block[policyIndex].data + offsetval), data,4); //save all this
                accessDRAM(addr, (cache[indexval].block[policyIndex].data), byte_size, READ);
                
            }
        }
        else if (memory_sync_policy == WRITE_THROUGH)/// write-through case no dirty case
        {
            int i =0; //hit
            while (i < assoc)
            {
                if (tagval == cache[indexval].block[i].tag ==1){
                    if(cache[indexval].block[i].valid == 1){ // a block hit
                        blockHit = 1;
                        cache[indexval].block[i].lru.value = 0;
                        cache[indexval].block[i].valid = 1;
                        cache[indexval].block[i].dirty = VIRGIN;
                        memcpy ((cache[indexval].block[i].data + offsetval),data, 4);
                        accessDRAM(addr, (cache[indexval].block[policyIndex].data), byte_size, WRITE);
                        
                        highlight_block(indexval, i);
                        highlight_offset(indexval, i, offsetval, HIT);
                    }
                }
                i++;
            }
            if (blockHit == 0) // miss SAME AS READ?
            {
                if (policy == RANDOM){ //random case which just sets index to any random number within assoc
                    policyIndex = randomint(assoc); //set index using random function given
                }
                else if (policy == LRU){
                    int i = 0; //init loop
                    while( i < assoc){
                        if(policyValue < cache[indexval].block[i].lru.value){ // prev value is less than potential current
                            policyIndex = i; //set to current iteration
                            policyValue = cache[indexval].block[i].lru.value; //update
                            
                        }
                        i++;//increment
                    }
                }
                
                accessDRAM(addr, (cache[indexval].block[policyIndex].data), byte_size, READ);
                cache[indexval].block[policyIndex].lru.value = 0;
                cache[indexval].block[policyIndex].valid = 1;
                cache[indexval].block[policyIndex].dirty = VIRGIN;
                cache[indexval].block[policyIndex].tag = tagval;
                memcpy ((cache[indexval].block[policyIndex].data + offsetval),data, 4); //save all this
                
                highlight_block(indexval, i-assoc);
                highlight_offset(indexval, i-assoc, offsetval, MISS);
            }
        }
    }
    
    /* This call to accessDRAM occurs when you modify any of the
     cache parameters. It is provided as a stop gap solution.
     At some point, ONCE YOU HAVE MORE OF YOUR CACHELOGIC IN PLACE,
     THIS LINE SHOULD BE REMOVED.
     */
    //accessDRAM(addr, (byte*)data, WORD_SIZE, we);
}
