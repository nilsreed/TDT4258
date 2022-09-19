#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADDRESS_BITS 32
#define MIN_CACHE_SIZE 128
#define MAX_CACHE_SIZE 4096

typedef enum { dm, fa } cache_map_t;
typedef enum { uc, sc } cache_org_t;
typedef enum { instruction, data } access_t;

typedef struct {
  uint32_t address;
  access_t accesstype;
} mem_access_t;

typedef struct {
  uint64_t accesses;
  uint64_t hits;
  // You can declare additional statistics if
  // you like, however you are now allowed to
  // remove the accesses or hits
} cache_stat_t;

// cache "line" struct to simplify searching in cache
typedef struct {
    uint16_t idx;
    uint32_t tag;
    access_t type;
    uint8_t valid;
} cache_line_t;

// DECLARE CACHES AND COUNTERS FOR THE STATS HERE

uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

// counters for fully associative cache FIFO queue
uint8_t d_entries = 0;  // data cache entries
uint8_t i_entries = 0;  // intstruction cache entries
uint8_t t_entries = 0;  // total cache entries

// USE THIS FOR YOUR CACHE STATISTICS
cache_stat_t cache_statistics;

/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE* ptr_file) {
  char buf[1000];
  char* token;
  char* string = buf;
  mem_access_t access;

  if (fgets(buf, 1000, ptr_file) != NULL) {
    /* Get the access type */
    token = strsep(&string, " \n");
    if (strcmp(token, "I") == 0) {
      access.accesstype = instruction;
    } else if (strcmp(token, "D") == 0) {
      access.accesstype = data;
    } else {
      printf("Unkown access type\n");
      exit(0);
    }

    /* Get the access type */
    token = strsep(&string, " \n");
    access.address = (uint32_t)strtol(token, NULL, 16);

    return access;
  }

  /* If there are no more entries in the file,
   * return an address 0 that will terminate the infinite loop in main
   */
  access.address = 0;
  return access;
}

int is_power_of_2(int n){
  return !(n & (n-1));
}

void main(int argc, char** argv) {
  // Reset statistics:
  memset(&cache_statistics, 0, sizeof(cache_stat_t));

  /* Read command-line parameters and initialize:
   * cache_size, cache_mapping and cache_org variables
   */
  /* IMPORTANT: *IF* YOU ADD COMMAND LINE PARAMETERS (you really don't need to),
   * MAKE SURE TO ADD THEM IN THE END AND CHOOSE SENSIBLE DEFAULTS SUCH THAT WE
   * CAN RUN THE RESULTING BINARY WITHOUT HAVING TO SUPPLY MORE PARAMETERS THAN
   * SPECIFIED IN THE UNMODIFIED FILE (cache_size, cache_mapping and cache_org)
   */
  if (argc != 4) { /* argc should be 2 for correct execution */
    printf(
        "Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] "
        "[cache organization: uc|sc]\n");
    exit(0);
  } else {
    /* argv[0] is program name, parameters start with argv[1] */

    /* Set cache size */
    cache_size = atoi(argv[1]);

    /* Set Cache Mapping */
    if (strcmp(argv[2], "dm") == 0) {
      cache_mapping = dm;
    } else if (strcmp(argv[2], "fa") == 0) {
      cache_mapping = fa;
    } else {
      printf("Unknown cache mapping\n");
      exit(0);
    }

    /* Set Cache Organization */
    if (strcmp(argv[3], "uc") == 0) {
      cache_org = uc;
    } else if (strcmp(argv[3], "sc") == 0) {
      cache_org = sc;
    } else {
      printf("Unknown cache organization\n");
      exit(0);
    }
  }

  /* Open the file mem_trace.txt to read memory accesses */
  FILE* ptr_file;
  ptr_file = fopen("mem_trace.txt", "r");
  if (!ptr_file) {
    printf("Unable to open the trace file\n");
    exit(1);
  }

  /* Check that the cache size is within limits and a power of 2 */
  if (cache_size < MIN_CACHE_SIZE){
    fprintf(stderr, "Error: Cache size too small!\n");
    exit(1);
  } else if (cache_size > MAX_CACHE_SIZE){
    fprintf(stderr, "Error: Cache size too big!\n");
    exit(1);
  } else if (!is_power_of_2(cache_size)){
    fprintf(stderr, "Error: Cache size not a power of 2!\n");
    exit(1);
  }
  // Figure out number of bits dedicated to tag and index, allocate cache and clear it

  uint8_t block_offset_bits = 6;                                      // Constant, as block size is 64
  uint32_t blocks = cache_size / block_size;                          // No. of blocks in the cache
  uint16_t index_bits = log2(blocks);                                 // log2 of no. of blocks is the number of bits needed for index
  uint16_t tag_bits = ADDRESS_BITS - block_offset_bits - index_bits;  // The rest of the bits in the address are given to the tag
  
  // If the cache is split, the indices need to reflect only addressing half the cache
  // This is equivalent to removing one bit for the indices, as this halves the number
  // of values the index can represent
  // This leaves a "leftover bit" which the tag absorbs.
  if (cache_org == sc){
    index_bits--;
    tag_bits++;
  }

  cache_line_t* cache = (cache_line_t*) malloc(cache_size*sizeof(cache_line_t));
  memset(cache, 0, blocks*sizeof(cache_line_t));


/*
(cache_line_t) {
              .idx = index,
              .tag = tag,
              .type = access.accesstype,
              .valid = 1
              };
*/


  /* Loop until whole trace file has been read */
  mem_access_t access;
  while (1) {
    access = read_transaction(ptr_file);
    // If no transactions left, break out of loop
    if (access.address == 0) break;
    printf("%d %x\n", access.accesstype, access.address);
    /* Do a cache access */
    /**
     * TODO
     *  find tag, idx
     *  search cache
     *    differently based on parameters
     *  update cache if not found
     *    differently based on parameters
     *  update cache_statistics
     */
    
    // Find index and tag for searching in the cache
    uint32_t tag = access.address >> (ADDRESS_BITS - tag_bits);
    uint32_t index = access.address << (tag_bits);
    index = index >> (tag_bits + block_offset_bits);

    uint32_t offset = 0;               // offset to be used if split cache is used
    uint32_t search_length = blocks ;  // If fully associative split cache is used, only half the
                                      // cache should be searched through. To acheive a more
                                      // general solution, this variable is used to keep track of
                                      // of how many cache entries that are to be searched through
                                      // Note: in the case of a direct mapped cache, this variable is unused
    if (cache_org == sc){
      if (cache_mapping == dm) {

      } else { // cache_mapping == fa

      }
    } else { // cache_org == uc
      if (cache_mapping == dm) {
        // If the cache is unified and direct mapped there is only one place
        // we need to check. If the tag is correct, the acces type is correct
        // and the entry is valid, we have a cache hit. In all other cases, we
        // have a cache miss, and we can simply "retrieve" the "data" we're
        // looking for and put it in the cache.
        if (cache[index].tag == tag && \
            cache[index].type == access.accesstype && \
            cache[index].valid){ // hit
          cache_statistics.hits++;
        } else { // miss
          // "Retrieve" "data" into cache
          cache[index] = (cache_line_t) {
            .idx = index,
             .tag = tag,
             .type = access.accesstype,
             .valid = 1
             };
        }
      } else { // cache_mapping == fa
        int hit = 0;
        for (int i = 0; i < cache_size; i++){
          // When the cache is fully associative, we say that the index
          // is 0 bits long and that the address is divided into tag
          // and block offset. Thus one should only chech the tag.
          // In this implementation I instead compare index and tag
          // for compatibility with the direct mapped case. This way
          // the tag and index don't need to change size.
          // The result is the same.
          // If there index and tag match, we either have a hit
          // or we need to invalidate the cache block because it
          // has the wrong type
          if (cache[i].idx == index && cache[i].tag == tag){
            if (cache[i].type == access.accesstype){
              hit = 1;
              cache_statistics.hits++;
              break;
            } else {
              // Here we need to invalidate the cache entry with
              // correct address but wrong data type. In practice
              // we just advance the FIFO queue over it using memset
              memcpy(cache + i, cache + i + 1, cache_size - i - 1);
            }
          }
        }
      }
    }
    // Increment number of accesses once per loop
    cache_statistics.accesses++;
  }

  // Free malloc'd cache
  free(cache);

  /* Print the statistics */
  // DO NOT CHANGE THE FOLLOWING LINES!
  printf("\nCache Statistics\n");
  printf("-----------------\n\n");
  printf("Accesses: %ld\n", cache_statistics.accesses);
  printf("Hits:     %ld\n", cache_statistics.hits);
  printf("Hit Rate: %.4f\n",
         (double)cache_statistics.hits / cache_statistics.accesses);
  // DO NOT CHANGE UNTIL HERE
  // You can extend the memory statistic printing if you like!

  /* Close the trace file */
  fclose(ptr_file);
}