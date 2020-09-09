 /* diskget.c
  *
  * Ryan Russell
  * V00873387
  * July 24th, 2020
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
	
	// Ensure correct command line input
	if (argc < 3 || argc > 3) {
		fprintf(stderr, "Error: incorrect command line input. Please include file to retrieve.\n");
		exit(EXIT_FAILURE);
	}

	struct stat buffer;
	int fp = open(argv[1], O_RDWR);
	char* desired_file = argv[2];

	if (fp < 0) {
		fprintf(stderr, "Error: failed to open file.\n");
		exit(EXIT_FAILURE);
	}
	
	// Retrieve info about input file..
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
	int num_entries = 0;
	int file_exists = 0;
	struct RootDirectoryEntry file_entries[1];
	
	// Set up variables to retrieve root directory block data.
	uint32_t root_start = superblock->start_block_root * superblock->block_size;
	uint32_t root_end = root_start + (superblock->num_blocks_root * superblock->block_size);
	uint32_t root_cur = root_start;
	uint32_t cur_entry = 0;
	
	// Loop through directory entries to look for desired file.
	while (root_cur < root_end) {
		
		struct RootDirectoryEntry* dir_entry = malloc(sizeof(*dir_entry));
		struct Datetime* datetime = malloc(sizeof(*datetime));
		uint8_t status = *(uint8_t*)&map[root_cur];
		
		// Set the status only if a file is found, otherwise continue..
		if (status == 0x3) {
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
		
		// If the current directory entry is the desired file, obtain the file's info.	
		if (strcmp(desired_file, dir_entry->filename) == 0) {

			file_exists = 1;

			dir_entry->start_block = ntohl(*(uint32_t*)&map[root_cur + DIRECTORY_START_BLOCK_OFFSET]);
			dir_entry->num_blocks = ntohl(*(uint32_t*)&map[root_cur + DIRECTORY_BLOCK_COUNT_OFFSET]);

			// Save the current directory entry struct in the array.
			file_entries[num_entries] = *dir_entry;
			num_entries++;
		}

		free(dir_entry);
		free(datetime);
		root_cur += 64;
		cur_entry++;
	}
	
	if (!file_exists) {
		fprintf(stderr, "File not found.\n");
		exit(EXIT_FAILURE);
	}
	

	FILE* ofp;
	char* new_filename = malloc(sizeof(desired_file));
       	strcpy(new_filename, desired_file);	
	ofp = fopen(new_filename, "w");
	
	int data_location = file_entries[0].start_block * superblock->block_size;
	char file_contents[superblock->block_size];
	int last_block_size = 0;
	int size = file_entries[0].file_size;
	
	// If the file is contained in a single block, make sure to only write the relevant data.
	if (file_entries[0].num_blocks > 1) {

		// Set the size of the data in the last block of the file properly.
		last_block_size = size - ((file_entries[0].num_blocks - 1) * superblock->block_size);
		
		// Write the first block of data for the case of a multi-block file.
		strncpy(file_contents, (char*)(uint8_t*)&map[data_location], superblock->block_size);
		fwrite(file_contents, 1, superblock->block_size, ofp);
	} else {
		
	       last_block_size = size;

	       // Write all of the data since the file is contained in a single block.
	       strncpy(file_contents, (char*)(uint8_t*)&map[data_location], last_block_size);
	       fwrite(file_contents, 1, last_block_size, ofp);
	}
	

	int FAT_entry = (file_entries[0].start_block * 4) + (superblock->block_size * superblock->start_block_fat);
	uint64_t FAT_entry_data = ntohl(*(uint64_t*)&map[FAT_entry]);
	int FAT_entry_val = FAT_entry_data;
	int current_block = 1;
	
	// Check the current FAT entry, write the current block's data, and check the next 
	// FAT entry until the whole file has been copied over to the current Unix directory.
	while(FAT_entry_val != FAT_EOF) {

		int next_location = FAT_entry_val * superblock->block_size;
		char next_contents[superblock->block_size];
		
		if (current_block + 1 == file_entries[0].num_blocks) {
			strncpy(next_contents, (char*)(uint8_t*)&map[next_location], last_block_size);
			fwrite(next_contents, 1, last_block_size, ofp);
		} else {
			strncpy(next_contents, (char*)(uint8_t*)&map[next_location], superblock->block_size);
			fwrite(next_contents, 1, superblock->block_size, ofp);
		}

		current_block++;
		FAT_entry = (FAT_entry_val * 4) + (superblock->block_size * superblock->start_block_fat);
		FAT_entry_data = ntohl(*(uint64_t*)&map[FAT_entry]);
		FAT_entry_val = FAT_entry_data;
	}

	printf("File copied successfully.\n");

	free(new_filename);
	free(superblock);
	fclose(ofp);
	close(fp);
	return 0;
}
