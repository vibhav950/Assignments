/**
 * dm_cache.c - program to simulate a direct-mapped cache.
 *
 * For each memory reference,  a cache hit/miss is evaluated
 * and a write operation is performed if required. The cache
 * memory block is displayed on every iteration.
 *
 * CS251B (MPCA) - Assignment 2
 * Dated 18-4-24
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/* Defines for the memory and cache sizes */
#define WORD_SIZE 4         /* 4 byte word */
#define BLOCK_SIZE (4 << 2) /* 16 byte block (4 words) */
#define CACHE_SIZE 256      /* 256 byte cache */
#define MEM_SIZE 4096       /* 4096 byte memory */

#define N_BLOCKS (MEM_SIZE / BLOCK_SIZE)
#define N_LINES (CACHE_SIZE / BLOCK_SIZE)

/* Defines for the physical address bit split */
#define N_ADDRESS_BITS 10     /* log2(MEM_SIZE / WORD_SIZE) */
#define N_BLOCK_OFFS_BITS 2   /* log2(WORD_SIZE) */
#define N_BLOCK_NUMBER_BITS 8 /* log2(MEM_SIZE / BLOCK_SIZE) */
#define N_LINE_NUMBER_BITS 4  /* log2(CACHE_SIZE / BLOCK_SIZE) */
#define N_TAG_BITS (N_ADDRESS_BITS - N_BLOCK_OFFS_BITS - N_LINE_NUMBER_BITS)

#define OFFS(addr) ((addr) & ((1 << N_BLOCK_OFFS_BITS) - 1))           /* Get the offset value from the PA */
#define BLOCK(addr) ((addr) >> N_BLOCK_OFFS_BITS)                      /* Get the block idx from the PA */
#define LINE(addr) (BLOCK(addr) & ((1 << N_LINE_NUMBER_BITS) - 1))     /* Get the line idx from the PA */
#define TAG(addr) ((addr) >> (N_BLOCK_OFFS_BITS + N_LINE_NUMBER_BITS)) /* Get the tag bits from the PA */

#define ceil_div(a, b) ((a) / (b) + ((a) % (b) ? 1 : 0))

typedef struct _cache_node_t
{
    unsigned int tag : N_TAG_BITS;
    unsigned int block : N_BLOCK_NUMBER_BITS;
    unsigned int offset : N_BLOCK_OFFS_BITS;
    unsigned int valid : 1;
    unsigned int mem[BLOCK_SIZE];
} cache_node_t;

/* Global counter values for cache history */
static int hits = 0;
static int misses = 0;
static int evictions = 0;

/* The cache */
static cache_node_t cache[N_LINES];

/* The main memory */
static unsigned int mem32[MEM_SIZE / WORD_SIZE];

char *toBinaryString(unsigned char tag, int nbits)
{
    char *binval = (char *)malloc((nbits + ((nbits - 1) / 3) + 1) * sizeof(char));
    int i, j = 0;

    for (i = nbits - 1; i >= 0; i--) {
        binval[j++] = (tag & (1 << i)) ? '1' : '0';
        if (i % 4 == 0 && i != 0)
            binval[j++] = ' ';
    }

    binval[j] = '\0';

    return binval;
}

void print_cache()
{
    /* Index - Cache line index
       Validity bit - garbage or valid value?
       Tag bits - used to differentiate blocks mapped to the same line
       Block - the mem block currently stored in the cache line
       Offset - offset of the word in the block
       Address - physical address of the word in decimal
       Value - content of word at given address
    */
    printf("+--------+--------+--------+--------+--------+--------+--------+\n");
    printf("| Index  | Valid  | Tag    | Block  | Offset | Word   | Value  |\n");
    printf("+--------+--------+--------+--------+--------+--------+--------+\n");

                
    for (int i = 0; i < N_LINES; i++) {
        printf("| %6d | %6d |  %s  | %6d |   %2d   | %6d | %6d |\n",
                i,
                cache[i].valid,
                toBinaryString(cache[i].tag, N_TAG_BITS),
                cache[i].block,
                cache[i].offset,
                /* Construct the word addr from the block addr and block offset */
                cache[i].offset + (cache[i].block << N_BLOCK_OFFS_BITS),
                cache[i].mem[cache[i].offset]);
    }

    printf("+--------+--------+--------+--------+--------+--------+--------+\n");
}

void perform_read(uint32_t addr)
{
    uint8_t offset = OFFS(addr);
    uint8_t block = BLOCK(addr);
    uint8_t line = LINE(addr);
    uint8_t tag = TAG(addr);

    if (cache[line].valid == 0) {
        misses++;
        cache[line].tag = tag;
        cache[line].block = block;
        cache[line].offset = offset;
        cache[line].valid = 1;
        printf("Cache miss!\n");
        cache[line].mem[0] = mem32[block * (BLOCK_SIZE / WORD_SIZE)];
        cache[line].mem[1] = mem32[block * (BLOCK_SIZE / WORD_SIZE) + 1];
        cache[line].mem[2] = mem32[block * (BLOCK_SIZE / WORD_SIZE) + 2];
        cache[line].mem[3] = mem32[block * (BLOCK_SIZE / WORD_SIZE) + 3];
    } else if (cache[line].tag != tag) {
        misses++;
        evictions++;
        cache[line].tag = tag;
        cache[line].block = block;
        cache[line].offset = offset;
        cache[line].mem[0] = mem32[block * (BLOCK_SIZE / WORD_SIZE)];
        cache[line].mem[1] = mem32[block * (BLOCK_SIZE / WORD_SIZE) + 1];
        cache[line].mem[2] = mem32[block * (BLOCK_SIZE / WORD_SIZE) + 2];
        cache[line].mem[3] = mem32[block * (BLOCK_SIZE / WORD_SIZE) + 3];
        printf("Cache miss!\n");
    } else {
        hits++;
        cache[line].offset = offset;
        printf("Cache hit!\n");
    }

    printf("\n\x1B[94mValue\x1B[0m: %lu\n\n", cache[line].mem[offset]);
}

void perform_write(uint16_t addr, uint32_t word)
{
    uint8_t offset = OFFS(addr);
    uint8_t block = BLOCK(addr);
    uint8_t line = LINE(addr);
    uint8_t tag = TAG(addr);

    if (cache[line].valid == 0 || cache[line].tag != tag) {
        /* Block not present in cache, we perform write around */
        misses++;
        mem32[addr] = word;
        printf("Write miss!\n");
    } else {
        /* Perform write through */
        hits++;
        /* Write to cache */
        cache[line].mem[offset] = word;
        /* Write to memory */
        mem32[addr] = word;
        printf("Write hit!\n");
    }
}

int main()
{
    printf("Cache Simulator\n"
            "Mem size: 4096 B\n"
            "Cache size: 256 B\n"
            "Block size: 16 B\n"
            "Press Ctrl+C to exit\n\n");

    /* Init the cache */
    memset((char *)cache, 0, sizeof(cache_node_t) * N_LINES);

    while (1) {
        char cho;

        printf("r/w ");
        scanf(" %c", &cho);

        switch (cho) {
            case 'r':
            case 'R':
            {
                uint16_t address;

                printf("address ");
                scanf("%u", &address);

                /* Validate the address */
                if (address > 4095 / WORD_SIZE) {
                    printf("\x1B[91m[ERROR]\x1B[0m Address out of bounds\n\n");
                    continue;
                }

                /* Print binary breakdown */
                printf("\nPhysical address: %s\n", toBinaryString(address, ceil_div(N_ADDRESS_BITS, 4) << 2));
                printf("Offset bits: %s\n", toBinaryString(OFFS(address), N_BLOCK_OFFS_BITS));
                printf("Line number bits: %s\n", toBinaryString(LINE(address), N_LINE_NUMBER_BITS));
                printf("Tag bits: %s\n", toBinaryString(TAG(address), N_TAG_BITS));

                perform_read(address);

                print_cache();
                printf("\x1B[94mHits\x1B[0m: %d, \x1B[94mMisses\x1B[0m: %d, \x1B[94mEvictions\x1B[0m: %d\n\n",
                        hits,
                        misses,
                        evictions);

                break;
            }
            case 'w':
            case 'W':
            {
                uint16_t address;
                uint32_t word;

                printf("address ");
                scanf("%u", &address);

                /* Validate the address */
                if (address > 4095 / WORD_SIZE) {
                    printf("\x1B[91m[ERROR]\x1B[0m Address out of bounds\n\n");
                    continue;
                }

                printf("word ");
                scanf("%lu", &word);

                /* Validate the address */
                if (address > 4095 / WORD_SIZE) {
                    printf("\x1B[91m[ERROR]\x1B[0m Address out of bounds\n\n");
                    continue;
                }

                perform_write(address, word);

                for (int i = 0; i < MEM_SIZE / WORD_SIZE; ++i) {
                    if (i != 0 && i % 4 == 0)
                        printf("\n");
                    printf("%lu ", mem32[i]);
                }
                printf("\n\n");

                print_cache();
                printf("\x1B[94mHits\x1B[0m: %d, \x1B[94mMisses\x1B[0m: %d, \x1B[94mEvictions\x1B[0m: %d\n\n",
                        hits,
                        misses,
                        evictions);

                break;
            }
            default:
            printf("%c",cho);
            break;
        }
    }

    return 0;
}
