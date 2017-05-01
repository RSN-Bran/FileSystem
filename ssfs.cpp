#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "header.h"

//TO DO: Read entire blocks, rather than only what is needed. Create function to do this.



using namespace std;

FILE* fptr;

void _create(const string& fileName) {
	//Checks if the filename is too long. Quits if it is
	if(fileName.size() > 32) {
		fprintf(stderr, "File Name too long. Must be 32 characters or fewer.\n");
		return;
	}

	//Create a local copy of the inode map
	fseek(fptr, super.inodeMapLocation, SEEK_SET);
	int tempInodeMap[256];
	fread(tempInodeMap, 256*sizeof(int), 1, fptr);


	//Loop through local version of inode map.
	//If an index of inode map is full, look at the name of the file
	//If the name of the file is the same as the one to be added, quit.
	//Also save the index of the first free location in the inode map for later use
	int firstFree = -1;
	int counter = 0;
	for(int i = 0; i < 256; i++) {
		int tempBlock = tempInodeMap[i];
		if(tempBlock != -1) {
			inode currentInode;
			fseek(fptr, super.blockSize * tempBlock, SEEK_SET);
			//NOT READING WHOLE BLOCK
			fread(&currentInode, sizeof(inode), 1, fptr);
			if(currentInode.fileName == fileName) {
				fprintf(stderr, "A File with this name already exists\n");
				return;
			}
			counter++;
		}
		else if(firstFree == -1) {
			firstFree = i;
		}
	}

	//Check if there is any free space in the inode map. If there is not quit
	if(counter == 256) {
		fprintf(stderr, "File could not be created. 256 files already exist\n");
		return;
	}

	//All tests have been passed, proceed to create new inode in memory
	//Initialize all pointers to -1, as these files currently have no data
	inode newInode;
	for(int i = 0; i < fileName.size(); i++) {
		newInode.fileName[i] = fileName[i];
	}
	newInode.fileName[fileName.size()] = '\0';
	newInode.fileSize = 0;
	fill_n(newInode.directPointers, 12, -1);
	newInode.indirectPointer = -1;
	newInode.doubleIndirectPointer = -1;

	//Copy the entire freeList into a local array
	bool freeItemsInBlock[super.numBlocks];
	fseek(fptr, super.freeBlocksLocation, SEEK_SET);
	int nextFreeBlock = -1;

	//Loop through the local free list and record the first available block of memory
	//NEED TO READ THE WHOLE BLOCK, FIX LATER
	fread(freeItemsInBlock, super.numBlocks, 1, fptr);
	for(int i = 0; i < super.numBlocks && nextFreeBlock == -1; i++) {
		if(freeItemsInBlock[i] == false) {
			nextFreeBlock = i;
			freeItemsInBlock[i] = true;
		}
	}
	fseek(fptr, super.inodeMapLocation, SEEK_SET);

	//Update inodeMap with block num of newest Inode
	//Go to that block and write the inode
	tempInodeMap[firstFree] = nextFreeBlock;
	fseek(fptr, nextFreeBlock*super.blockSize, SEEK_SET);
	fwrite(&newInode, sizeof(inode), 1, fptr);
	fillBlockWithGarbage(fptr);

	//Rewrite inode map
	fseek(fptr, super.inodeMapLocation, SEEK_SET);
	fwrite(tempInodeMap, 256*sizeof(int), 1, fptr);
	fillBlockWithGarbage(fptr);

	//Rewrite freeList
	fseek(fptr, super.freeBlocksLocation, SEEK_SET);
	fwrite(freeItemsInBlock, super.numBlocks, 1, fptr);
	fillBlockWithGarbage(fptr);
	
}

void _import(string ssfsFile, string unixFile) {
}

void _cat(string fileName) {
}

void _delete(string fileName) {
}

void _write(string fileName, char c, int start, int numBytes) {
}

void _read(string fileName, int start, int numBytes) {
}

void _list() {
	//Create a local copy of the inode map
	fseek(fptr, super.inodeMapLocation, SEEK_SET);
	int tempInodeMap[256];
	fread(tempInodeMap, 256*sizeof(int), 1, fptr);

	//Loop through the iNode map, if the pointer is not to -1 then an inode exists
	for(int i = 0; i < 32; i++) {
		if(tempInodeMap[i] != -1) {
			//Seek to block where the inode is held, print out its name and size
			inode tempInode;
			fseek(fptr, super.blockSize*tempInodeMap[i], SEEK_SET);
			//NOT READING WHOLE BLOCK
			fread(&tempInode, sizeof(inode), 1, fptr);
			cout << "File Name: " << tempInode.fileName << "\tFile Size: " << tempInode.fileSize << endl;
		}
	}
}

void _shutdown() {
}

int main(int argc, char** argv) {
	string diskFileName;
	string fileArray[argc-2];
	if(argc == 1) {
		fprintf(stderr, "Input requires filename\n");
		exit(EXIT_FAILURE);
	}
	else if(argc > 6) {
		fprintf(stderr, "Too many threads\n");
		exit(EXIT_FAILURE);
	}
	else {
	}
	diskFileName = argv[1];
	for(int i = 2; i < argc; i++) {
		fileArray[i-2] = argv[i];
	}

	//Might need to change later, ask Mike
	fptr = fopen(diskFileName.c_str(), "rb+");
	fread(&super, sizeof(super), 1, fptr);

	ifstream thread0(fileArray[0].c_str());
	if(!thread0.is_open()) {
		fprintf(stderr, "Can't open file\n");
		exit(EXIT_FAILURE);	
	}
	string line;
	while(getline(thread0, line)) {
		string command;
		stringstream ss(line);
		ss>>command;
		if(command == "CREATE") {
			ss >> command;
			_create(command);
		}
		else if(command == "IMPORT") {
			string ssfsFile;
			string unixFile;
			ss >> ssfsFile >> unixFile;
			_import(ssfsFile, unixFile);
		}
		else if(command == "CAT") {
			ss >> command;
			_cat(command);
		}
		else if(command == "DELETE") {
			ss >> command;
			_delete(command);
		}
		else if(command == "WRITE") {
			string fileName;
			char c;
			int start;
			int numBytes;
			ss >> fileName >> c >> start >> numBytes;
			_write(fileName, c, start, numBytes);
		}
		else if(command == "READ") {
			string fileName;
			int start;
			int numBytes;
			ss >> fileName >> start >> numBytes;
			_read(fileName, start, numBytes);
		}
		else if(command == "LIST") {
			_list();
		}
		else if(command == "SHUTDOWN") {
			_shutdown();
		}		
		

	}
	thread0.close();
	
	
}
