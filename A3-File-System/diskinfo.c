/*
 * diskinfo.c
 *
 * Ryan Russell
 * V00873387
 * July 20th, 2020
 * CSC 360 Assignment 3
 * 
 * References:
 * [1] https://stackoverflow.com/questions/1864103/reading-superblock-into-a-c-structure
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "Constants.h"


// Struct to read superblock info into (idea from Stack Overflow) [1].
struct Superblock {
	uint16_t block_size;
	uint32_t block_count;
	uint32_t start_block_fat;
	uint32_t num_blocks_fat;
	uint32_t start_block_root;
	uint32_t num_blocks_root;
};


int main (int argc, char* argv[]) {
	
	// Ensure correct command line input.
	if (argc < 2 || argc > 2) {
		fprintf(stderr, "Error: incorrect command line input.\n");
		exit(EXIT_FAILURE);
	}
	
	struct stat buffer;
	int fp = open(argv[1], O_RDWR); 

	if (fp < 0) {
		fprintf(stderr, "Error: failed to open file.\n");
		exit(EXIT_FAILURE);
	}	
	
	// Retrieve info about input file.
	fstat(fp, &buffer);
	
	// Map memory from the disk image so it can be read into the superblock struct.
	char* map = (char*)mmap(NULL, buffer.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fp, 0);

	if (map == MAP_FAILED) {
		fprintf(stderr, "Error: failed memory map.\n");
		exit(EXIT_FAILURE);
	}
	
	
	// Create a struct and read the mapped memory into the struct (accounting for byte ordering).
	struct Superblock* superblock = malloc(sizeof(*superblock));
	superblock->block_size = ntohs(*(uint16_t*)&map[BLOCKSIZE_OFFSET]);
	superblock->block_count = ntohl(*(uint32_t*)&map[BLOCKCOUNT_OFFSET]);
	superblock->start_block_fat = ntohl(*(uint32_t*)&map[FATSTART_OFFSET]);
	superblock->num_blocks_fat = ntohl(*(uint32_t*)&map[FATBLOCKS_OFFSET]);
	superblock->start_block_root = ntohl(*(uint32_t*)&map[ROOTDIRSTART_OFFSET]);
	superblock->num_blocks_root = ntohl(*(uint32_t*)&map[ROOTDIRBLOCKS_OFFSET]);

	// Superblock info output.
	printf("Super block information:\n");
	printf("Block size: %d\n", superblock->block_size);
	printf("Block count: %d\n", superblock->block_count);
	printf("FAT starts: %d\n", superblock->start_block_fat);
	printf("FAT blocks: %d\n", superblock->num_blocks_fat);
	printf("Root directory start: %d\n", superblock->start_block_root);
	printf("Root directory blocks: %d\n", superblock->num_blocks_root);
	
	// Set up variables to retrieve FAT block data.
	uint32_t fat_start = superblock->start_block_fat * superblock->block_size;
	uint32_t fat_end = fat_start + (superblock->num_blocks_fat * superblock->block_size);
	uint32_t fat_cur = fat_start;
	
	int free_blocks = 0;
	int reserved_blocks = 0;
	int allocated_blocks = 0;
	
	// Retrieve FAT block data by looking through the entire FAT.
	while (fat_cur < fat_end) {
		
		uint32_t fat_val = ntohl(*(uint32_t*)&map[fat_cur]);
		
		if (fat_val == FAT_FREE) {
			free_blocks++;
		} else if (fat_val == FAT_RESERVED) {
			reserved_blocks++;
		} else {
			allocated_blocks++;
		}

		fat_cur += 4;
	}

	// FAT info output.
	printf("\nFAT information:\n");
	printf("Free Blocks: %d\n", free_blocks);
	printf("Reserved Blocks: %d\n", reserved_blocks);
	printf("Allocated Blocks: %d\n", allocated_blocks);

	free(superblock);
	close(fp);
	return 0;
}



