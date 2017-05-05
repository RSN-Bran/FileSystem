// 93 bytes, Min block size is 128
//Want to add unfinished block and unfinished byte ints
typedef struct {
	char fileName[33];
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
}
