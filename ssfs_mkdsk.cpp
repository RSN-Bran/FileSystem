#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

typedef struct {
	string fileName;
	int fileSize;
	int directPointers[12];
	int indirectPointer;
	int doubleIndirectPointer;
} inode;

typedef struct {
	int numBlocks;
	int blockSize;
	int blockNum;
}superBlock;

typedef struct {
	bool* freeBlocks;
}freeList;

typedef struct {
	inode* inodeMap[256];
}map;

int main(int argc, char** argv) {
	//Create the super block, set its index to 0
	superBlock super;
	super.blockNum = 0;

	//Parse input. Set default file name to 'DISK' if it is not provided. Throw error if too many or two few arguments are given
	string diskFileName;
	if(argc == 3) {
		diskFileName = "DISK";
	}
	else if(argc < 3) {
		fprintf(stderr, "Error\n");
		exit(EXIT_FAILURE);
	}
    	else {
		diskFileName = argv[3];
	}

	//Set values of the super block to the parsed values
	super.numBlocks = atoi(argv[1]);
	super.blockSize = atoi(argv[2]);

	//Check if NumBlocks is a Power of 2 //65536
	if(!(super.numBlocks && !(super.numBlocks & (super.numBlocks-1)))) {
		fprintf(stderr, "Error, NumBlocks must be a Power of 2\n");
		exit(EXIT_FAILURE);
	}
	if(!(super.numBlocks >= 1024 && super.numBlocks <= 131072)) {
		fprintf(stderr, "Error, NumBlocks must be between 1024 and 131072\n");
		exit(EXIT_FAILURE);
	}

	//Checks if Block Size is a Power of 2
	if(!(super.blockSize && !(super.blockSize & (super.blockSize-1)))) {
		fprintf(stderr, "Error, Block Size must be a Power of 2\n");
		exit(EXIT_FAILURE);
	}
	if(super.blockSize > 512 || super.blockSize < 128) {
		fprintf(stderr, "Error, Block Size must be between 128 and 512\n");
		exit(EXIT_FAILURE);
	}

	//Create file, truncate it to desired size, and close it.
	int fdes;
	fdes = open(diskFileName.c_str(), O_CREAT | O_WRONLY | O_TRUNC, "wb");

	if(ftruncate(fdes, super.numBlocks*super.blockSize) == -1) {
		perror("ftruncate() failed");
		exit(EXIT_FAILURE);
	}
	close(fdes);

	//Write the Number of Blocks and Block Size to the superblock portion of the file
	FILE* fptr = fopen(diskFileName.c_str(), "wb");
	fwrite(&super, sizeof(super), 1, fptr);
	fclose(fptr);

//	fptr = fopen(diskFileName.c_str(), "rb");
//	fread(&test, sizeof(super), 1, fptr);
//	fclose(fptr);
//	cout << test.numBlocks << " " <<test.blockSize << endl;
	

	return 0;
} 
