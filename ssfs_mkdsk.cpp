#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// put one inode per block because it's easy

//	fptr = fopen(diskFileName.c_str(), "rb");
//	fread(&test, sizeof(super), 1, fptr);
//	fclose(fptr);
//	cout << test.numBlocks << " " <<test.blockSize << endl;

using namespace std;

// 92 bytes
typedef struct {
	char fileName[32];
	int fileSize;
	int directPointers[12];
	int indirectPointer;
	int doubleIndirectPointer;
} inode;

typedef struct {
	int numBlocks;
	int blockSize;
	int blockNum;
	int inodeMapLocation;
	int freeBlocksLocation;
} superBlock;

superBlock super;

void fillBlockWithGarbage(FILE* fptr) {
    char garbage = 'a';
    
    int numOfBytesToWrite = super.blockSize - (ftell(fptr) % super.blockSize);
    
    if (numOfBytesToWrite != super.blockSize) {
        fwrite(&garbage, sizeof(char), numOfBytesToWrite, fptr);
    }
    
    //cout << numOfBytesToWrite << endl;
}

int main(int argc, char** argv) {
	int inodeMap[256];
	
	//Create the super block, set its index to 0
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
    
    // array of however many blocks there are in the file
    // false means free
	bool freeBlocks[super.numBlocks];
    
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
	
	// Write a character to the end of the file so the file size stays correct
	fseek(fptr, super.numBlocks*super.blockSize - 1, SEEK_SET);
	
	char a = 'a';
	
	fwrite(&a, sizeof(char), 1, fptr);
	
	// Seek to the location of the second block and write inode map
	fseek(fptr, super.blockSize, SEEK_SET);
	super.inodeMapLocation = ftell(fptr);
	fwrite(inodeMap, 256 * sizeof(int), 1, fptr);
	
	fillBlockWithGarbage(fptr);
	
	long fullBlockCount = (ftell(fptr) + super.numBlocks) / super.blockSize;
	
	//cout << fullBlockCount << endl;
	
	for (int i = 0; i < fullBlockCount; i++) {
	    freeBlocks[i] = true;
	}
	
	// Putting in free list
	super.freeBlocksLocation = ftell(fptr);
	fwrite(freeBlocks, super.numBlocks, 1, fptr);
	fillBlockWithGarbage(fptr);
	
	long freeBlocksEnd = ftell(fptr);
	
	// seek back to the beginning so we're at the right spot for writing the super block
	fseek(fptr, 0, SEEK_SET);
	fwrite(&super, sizeof(super), 1, fptr);
	fillBlockWithGarbage(fptr);
	
	// Jump back to right after the free blocks list
	fseek(fptr, freeBlocksEnd, SEEK_SET);
	
	//cout << ftell(fptr) << endl;
	
	// Insert the inodes into the file
	for (int i = 0; i < 256; i++) {
	    inode in;
	    
	    in.fileSize = 0;
	    in.indirectPointer = 0;
	    in.doubleIndirectPointer = 0;
	    
	    fwrite(&in, sizeof(in), 1, fptr);
	    fillBlockWithGarbage(fptr);
	}
	
	//cout << ftell(fptr) << endl;
	
	fclose(fptr);

	return 0;
} 
