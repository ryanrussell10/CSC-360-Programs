 /* disklist.c
 *
 * Ryan Russell
 * V00873387
 * July 22nd, 2020
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


// Struct to store datetimes for modified and created date.
struct Datetime {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};


// Struct to read root directory entries into.
struct RootDirectoryEntry {
	char status;
	uint32_t start_block;
	uint32_t num_blocks;
	uint32_t file_size;
	struct Datetime created_time;
	struct Datetime modified_time;
	char filename[31];
	uint8_t unused[6];
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
	
	// We know there are 8 directory entries per 512 byte block.
	int total_entries = superblock->num_blocks_root * 8;
	int num_entries = 0;
	struct RootDirectoryEntry dir_entries[total_entries];

	// Set up variables to retrieve root directory block data.
	uint32_t root_start = superblock->start_block_root * superblock->block_size;
	uint32_t root_end = root_start + (superblock->num_blocks_root * superblock->block_size);
	uint32_t root_cur = root_start;
	uint32_t cur_entry = 0;
	
	// Retrieve relevant root directory entry data.
	while (root_cur < root_end) {
		
		struct RootDirectoryEntry* dir_entry = malloc(sizeof(*dir_entry));
		struct Datetime* datetime = malloc(sizeof(*datetime));
		uint8_t status = *(uint8_t*)&map[root_cur];

		// Set the status based on the bits that indicate whether the entry is a file or not.
		if (status == 0x5) {
			dir_entry->status = 'D';
	       	} else if (status == 0x3) {
			dir_entry->status = 'F';
	       	} else {
			free(dir_entry);
			free(datetime);
			root_cur += 64;
			cur_entry++;
			continue;
		}
		
		dir_entry->file_size = ntohl(*(uint32_t*)&map[root_cur + DIRECTORY_FILE_SIZE_OFFSET]);
		strncpy(dir_entry->filename, (char*)(uint8_t*)&map[root_cur + DIRECTORY_FILENAME_OFFSET], 31);
		
		// Retrieve the modified time for this directory entry.
		datetime->year = ntohs(*(uint16_t*)&map[root_cur + DIRECTORY_MODIFY_OFFSET]);
	        datetime->month = *(uint8_t*)&map[root_cur + 22];
	        datetime->day = *(uint8_t*)&map[root_cur + 23];
	        datetime->hour = *(uint8_t*)&map[root_cur + 24];
	        datetime->minute = *(uint8_t*)&map[root_cur + 25];
	        datetime->second = *(uint8_t*)&map[root_cur + 26];
		dir_entry->modified_time = *datetime;
		
		// Save the current directory entry struct in the array.
		dir_entries[cur_entry] = *dir_entry;
		num_entries++;

		free(dir_entry);
		free(datetime);
		root_cur += 64;
		cur_entry++;
	}
	
	// Print root directory output data.
	int i = 0;
	for (i = 0; i < num_entries; i++) {
		printf("%c ", dir_entries[i].status);
		printf("%10d ", dir_entries[i].file_size);
		printf("%30s ", dir_entries[i].filename);
		printf("%4d/%02d/%02d %02d:%02d:%02d\n", 
			dir_entries[i].modified_time.year,
			dir_entries[i].modified_time.month,
			dir_entries[i].modified_time.day,
			dir_entries[i].modified_time.hour,
			dir_entries[i].modified_time.minute,
			dir_entries[i].modified_time.second);
	}

	free(superblock);
	close(fp);
	return 0;
}
