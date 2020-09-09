 /* diskput.c
  *
  * Ryan Russell
  * V00873387
  * July 30th, 2020
  * CSC 360 Assignment 3
  *
  * References:
  * [1] https://stackoverflow.com/questions/1864103/reading-superblock-into-a-c-structure
  * [2] https://www.tutorialspoint.com/c_standard_library/c_function_ftell.htm
  * [3] https://stackoverflow.com/questions/1442116/how-to-get-the-date-and-time-values-in-a-c-program
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

	if (argc < 3 || argc > 3) {
		fprintf(stderr, "Error: incorrect command line input. Please include file to retrieve.\n");
		exit(EXIT_FAILURE);
	}

	struct stat buffer;
	int fp = open(argv[1], O_RDWR);

	if (fp < 0) {
		fprintf(stderr, "Error: failed to open image file.\n");
		exit(EXIT_FAILURE);
	}
	
	fstat(fp, &buffer);
	
	// Open the file from Unix for reading and copying data.
	char* desired_file = argv[2];
	FILE* unix_file;
	unix_file = fopen(desired_file, "r");

	if (unix_file == NULL) {
		fprintf(stderr, "Error: failed to open specified file.\n");
		fprintf(stderr, "File not found.\n");
		exit(EXIT_FAILURE);
	}

	// Retrieve file size (idea from Tutorialspoint) [2].
	fseek(unix_file, 0, SEEK_END);
	int unix_file_size = ftell(unix_file);
	printf("File Size: %d\n\n", unix_file_size);
	//char* desired_file_data = malloc(sizeof(char) * unix_file_size);	
	fseek(unix_file, 0, SEEK_SET);

	char* map = (char*)mmap(NULL, buffer.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fp, 0);

	if (map == MAP_FAILED) {
		fprintf(stderr, "Error: failed memory map.\n");
		exit(EXIT_FAILURE);
	}

	struct Superblock* superblock = malloc(sizeof(*superblock));
	superblock->block_size = ntohs(*(uint16_t*)&map[BLOCKSIZE_OFFSET]);
	superblock->block_count = ntohl(*(uint32_t*)&map[BLOCKCOUNT_OFFSET]);
	superblock->start_block_fat = ntohl(*(uint32_t*)&map[FATSTART_OFFSET]);
	superblock->num_blocks_fat = ntohl(*(uint32_t*)&map[FATBLOCKS_OFFSET]);
	superblock->start_block_root = ntohl(*(uint32_t*)&map[ROOTDIRSTART_OFFSET]);
	superblock->num_blocks_root = ntohl(*(uint32_t*)&map[ROOTDIRBLOCKS_OFFSET]);

	// Read file from Unix into memory.
	//fread(desired_file_data, sizeof(char), unix_file_size, unix_file);	
	
	// Set the directory entry parameters.
	struct RootDirectoryEntry* dir_entry = malloc(sizeof(*dir_entry));
	dir_entry->file_size = unix_file_size;
	
	if (unix_file_size % superblock->block_size == 0) { 
       		dir_entry->num_blocks = (unix_file_size / superblock->block_size);
	} else {
		dir_entry->num_blocks = (unix_file_size / superblock->block_size) + 1;
	}

	dir_entry->status = 'F';	
	strncpy(dir_entry->filename, argv[2], 31);
	struct Datetime* create_time = malloc(sizeof(*create_time));
	struct Datetime* modify_time = malloc(sizeof(*modify_time));

	// Set the created time (code from Stack Overflow) [3].
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	create_time->year = tm.tm_year + 1900;
	modify_time->year = tm.tm_year + 1900;
	create_time->month = tm.tm_mon + 1;
	modify_time->month = tm.tm_mon + 1;
	create_time->day = tm.tm_mday;
	modify_time->day = tm.tm_mday;
	create_time->hour = tm.tm_hour;
	modify_time->hour = tm.tm_hour;
	create_time->minute = tm.tm_min;
	modify_time->minute = tm.tm_min;
	create_time->second = tm.tm_sec;
	modify_time->second = tm.tm_sec;	

	dir_entry->created_time = *create_time;
	dir_entry->modified_time = *modify_time;
	
	char desired_file_data[dir_entry->num_blocks][superblock->block_size];
	memset(desired_file_data, 0, dir_entry->num_blocks * superblock->block_size * sizeof(char));
	// Read file from Unix into memory.
	int i = 0;
	for (i = 0; i < dir_entry->num_blocks; i++) {
		
		if (i + 1 == dir_entry->num_blocks) {
			fread(desired_file_data[i], sizeof(char), unix_file_size, unix_file);	
		} else {
			fread(desired_file_data[i], sizeof(char), superblock->block_size, unix_file);
		}
	}

	uint32_t fat_start = superblock->start_block_fat * superblock->block_size;
	uint32_t fat_end = fat_start + (superblock->num_blocks_fat * superblock->block_size);
	uint32_t fat_cur = fat_start;
	uint32_t cur_fat_entry = 0;
	uint32_t prev_fat_entry = 0;
	int cur_block = 0;
	
	// Loop through the FAT looking for free locations to write the data to.
	while (fat_cur < fat_end && cur_block < dir_entry->num_blocks) {
	
		uint32_t fat_val = ntohl(*(uint32_t*)&map[fat_cur]);
		
		if (fat_val == FAT_FREE) {
			
			printf("Writing data for block %d:\n", cur_block);
			uint32_t data_write_offset = cur_fat_entry * superblock->block_size;
			uint32_t fat_write_offset = (prev_fat_entry * FAT_ENTRY_SIZE) + fat_start;
			uint32_t write_offset_eof = (cur_fat_entry * FAT_ENTRY_SIZE) + fat_start; 

			if (cur_block == 0) {	
				
				dir_entry->start_block = cur_fat_entry;
				printf("Starting block for the file will be: %d\n", cur_fat_entry);
				lseek(fp, data_write_offset, SEEK_SET);
				printf("Writing data to location: 0x%x\n", data_write_offset);
			      	write(fp, desired_file_data[cur_block], superblock->block_size);

				if (dir_entry->num_blocks == 1) {

					// Write EOF to FAT table.
				    	printf("Writing FAT value 0x%x to location: 0x%x\n", FAT_EOF, write_offset_eof);
					(*(uint32_t*)&map[write_offset_eof]) = htonl(FAT_EOF);
				
				}	

			} else if (cur_block == dir_entry->num_blocks - 1) {
				
				// Write last block of file data.
				lseek(fp, data_write_offset, SEEK_SET);
				printf("Writing data to location: 0x%x\n", data_write_offset);
				write(fp, desired_file_data[cur_block], superblock->block_size);

				// Write current FAT entry into data for previous FAT entry.
				printf("Writing FAT value 0x%x to location: 0x%x\n", cur_fat_entry, fat_write_offset);
				(*(uint32_t*)&map[fat_write_offset]) = htonl(cur_fat_entry);

				// Write EOF to FAT table.
				printf("Writing FAT value 0x%x to location: 0x%x\n", FAT_EOF, write_offset_eof);
				(*(uint32_t*)&map[write_offset_eof]) = htonl(FAT_EOF);

			} else {
				
				lseek(fp, data_write_offset, SEEK_SET);
				printf("Writing data to location: 0x%x\n", data_write_offset);
				write(fp, desired_file_data[cur_block], superblock->block_size);

				// Write current FAT entry into data for previous FAT entry.
				printf("Writing FAT value 0x%x to location: 0x%x\n", cur_fat_entry, fat_write_offset);
				(*(uint32_t*)&map[fat_write_offset]) = htonl(cur_fat_entry);

			}
			
			printf("\n");	
			cur_block++;
			prev_fat_entry = cur_fat_entry;
		}

		cur_fat_entry++;
		fat_cur += 4;
	}

	uint32_t root_start = superblock->start_block_root * superblock->block_size;
	uint32_t root_end = root_start + (superblock->num_blocks_root * superblock->block_size);
	uint32_t root_cur = root_start;
	uint32_t cur_entry = 0;
	int entry_available = 0;
	
	// Look for an available directory entry and write if we find one.
	while (root_cur < root_end) {
		
		uint8_t status = *(uint8_t*)&map[root_cur];
		if (status == 0x0) {

			entry_available = 1;
			printf("Writing directory entry info to location: 0x%x\n", root_cur);	

			// Write directory entry data into available entry.
			(*(uint8_t*)&map[root_cur + DIRECTORY_STATUS_OFFSET]) = 0x3;
			(*(uint32_t*)&map[root_cur + DIRECTORY_START_BLOCK_OFFSET]) = htonl(dir_entry->start_block);	
			(*(uint32_t*)&map[root_cur + DIRECTORY_BLOCK_COUNT_OFFSET]) = htonl(dir_entry->num_blocks);
			(*(uint32_t*)&map[root_cur + DIRECTORY_FILE_SIZE_OFFSET]) = htonl(dir_entry->file_size);
			(*(uint16_t*)&map[root_cur + DIRECTORY_CREATE_OFFSET]) = htons(dir_entry->created_time.year);
			(*(uint8_t*)&map[root_cur + 15]) = dir_entry->created_time.month;
			(*(uint8_t*)&map[root_cur + 16]) = dir_entry->created_time.day;
			(*(uint8_t*)&map[root_cur + 17]) = dir_entry->created_time.hour;
			(*(uint8_t*)&map[root_cur + 18]) = dir_entry->created_time.minute;
			(*(uint8_t*)&map[root_cur + 19]) = dir_entry->created_time.second;
			(*(uint16_t*)&map[root_cur + DIRECTORY_MODIFY_OFFSET]) = htons(dir_entry->modified_time.year);
			(*(uint8_t*)&map[root_cur + 22]) = dir_entry->modified_time.month;
			(*(uint8_t*)&map[root_cur + 23]) = dir_entry->modified_time.day;
			(*(uint8_t*)&map[root_cur + 24]) = dir_entry->modified_time.hour;
			(*(uint8_t*)&map[root_cur + 25]) = dir_entry->modified_time.minute;
			(*(uint8_t*)&map[root_cur + 26]) = dir_entry->modified_time.second;
			
			lseek(fp, root_cur + DIRECTORY_FILENAME_OFFSET, SEEK_SET);
			write(fp, dir_entry->filename, 31);

			(*(uint32_t*)&map[root_cur + 58]) = htonl(FAT_EOF);
			(*(uint16_t*)&map[root_cur + 62]) = htons(0xFFFF);

			break;
		}

		root_cur += DIRECTORY_ENTRY_SIZE;
		cur_entry++;
	}

	if (!entry_available) {
		fprintf(stderr, "Error: no directory entry available for a file to be copied\n.");
		exit(EXIT_FAILURE);
	}
	
	printf("File copied to FAT file system successfully.\n");

	free(superblock);
	free(create_time);
	free(modify_time);
	fclose(unix_file);
	close(fp);
	return 0;
}
