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
