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


/*LIST OF FUNCTIONS:
CREATE: DONE
IMPORT: NOT STARTED
CAT: PARTIALLY DONE
			ONLY PRINTS DATA IN DIRECT POINTERS
DELETE: COMPLETE LOGIC, HAS BUGS (DEPENDS ON WRITE), COULD COPY SOME MORE LOGIC INTO SEPERATE FUNCTIONS
WRITE: NEEDS REWORK, CURRENTLY DOES DIRECT POINTERS, BUT LOGIC NEEDS OVERHAUL
READ: NOT STARTED
LIST: DONE
SHUTDOWN: NOT STARTED

*/

//TO DO: Read entire blocks, rather than only what is needed. Create function to do this.



using namespace std;

FILE* fptr;

int getAFreeBlock() {
	//Create a local copy of the free block list
	fseek(fptr, super.freeBlocksLocation, SEEK_SET);
	bool tempFreeBlocks[super.numBlocks];
	fread(tempFreeBlocks, super.numBlocks*sizeof(bool), 1, fptr);

	//Loop through the free block list. Stop looping and save index once an open slot is found
	int tempInd = -1;
	for(int i = 0; i < super.numBlocks && tempInd == -1; i++) {
		if(tempFreeBlocks[i] == false) {
			tempFreeBlocks[i] = true;
			tempInd = i;
		}
	}

	//Write back the updated free block list to the disk
	fseek(fptr, super.freeBlocksLocation, SEEK_SET);
	fwrite(tempFreeBlocks, super.numBlocks, 1, fptr);
	fillBlockWithGarbage(fptr);

	//Return the index of the free block
	cout << tempInd << endl;
	return tempInd;
}

//Takes in an array which represents blocks, which could come from direct pointers, indirect pointers, or double indirect pointers
//Loops through this array and writes data to non-empty locations
void writeData(inode& currentInode, int loops, int& numBytes, char c, int* pointers, int start) {
	currentInode.fileSize += numBytes;
	for(int i = 0; i < loops && numBytes != 0; i++) {
		if(pointers[i] == -1) {
			int freeBlock = getAFreeBlock();
			fseek(fptr, super.blockSize*freeBlock, SEEK_SET);
			//If the number of bytes is greater than the block size, fill the whole block with the character
			//No need to fill with garbage
			//Decrement the numBytes by the size of the block
			if(numBytes >= super.blockSize) {
				char arrayToWrite[super.blockSize];
				fill_n(arrayToWrite, super.blockSize, c);
				fwrite(&arrayToWrite, sizeof(arrayToWrite), 1, fptr);
				numBytes -= super.blockSize;
			}
			//Partially fill block with the char, then add a Null Terminator, then fill the rest with garbage and set numBytes to 0
			else{
				char arrayToWrite[numBytes+1];
				fill_n(arrayToWrite, numBytes, c);
				arrayToWrite[numBytes] = '\0';
				fwrite(&arrayToWrite, sizeof(arrayToWrite), 1, fptr);

				fillBlockWithGarbage(fptr);
				numBytes = 0;
			}
			pointers[i] = freeBlock;
		}
		//Need to check if a block with some info is not completely full, so we can continue to write to it
		else {
			
		}
	}
}

void deleteData(inode& currentInode, int loops, int* pointers, bool* tempFreeBlocks) {
	//Loop through the Pointers and delete everything in them
	for(int i = 0; i < loops; i++) {
		//Found an occupied direct pointer, seek to the block, delete everything in it
		//Set the direct pointer to -1, and the free block location to free
		if(pointers[i] != -1) {
			int block = pointers[i];
			//Not actually deleting anything, just removing pointer
			tempFreeBlocks[block] = false;
			pointers[i] = -1;
		}
	}
}

//COMPLETE
//Check inodes and create a new file at the first free inode.
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

	//Call get a free block to get the next free block
	int nextFreeBlock = getAFreeBlock();

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
	
}

void _import(string ssfsFile, string unixFile) {
}

void _cat(string fileName) {
	//Create a local copy of the inode map
	fseek(fptr, super.inodeMapLocation, SEEK_SET);
	int tempInodeMap[256];
	fread(tempInodeMap, 256*sizeof(int), 1, fptr);

	//Loop through INode Map, if an inode is present at an index, traverse it
	//If the traversed iNode has the same name as desired fileName, save its index and exit the loop
	int iNodeInd = -1;
	inode currentInode;
	for(int i = 0; i < 32 && iNodeInd == -1; i++) {
		int tempBlock = tempInodeMap[i];
		if(tempBlock != -1) {
			fseek(fptr, super.blockSize * tempBlock, SEEK_SET);
			//NOT READING WHOLE BLOCK
			fread(&currentInode, sizeof(inode), 1, fptr);
			if(currentInode.fileName == fileName) {
				iNodeInd = i;
			}
		}
	}
	//If the file is not found, print an error message and quit
	if(iNodeInd == -1) {
		fprintf(stderr, "File is not present in File System\n");
		return;
	}

	//Traverse through direct Pointers, stop reading if -1 is ever reached
	bool reading = true;
	for(int i = 0; i < 12 && reading; i++) {
		if(currentInode.directPointers[i] == -1) {
			reading = false;
		}
		//Create a buffer, seek to each block and write each character to the buffer until the block is exhausted or a NULL terminator is found
		else {
			char test;
			fseek(fptr, super.blockSize * currentInode.directPointers[i], SEEK_SET);
			for(int j = 0; j < super.blockSize; j++) {
				fread(&test, sizeof(char), 1, fptr);
				cout << test;
				if(test == '\0') {
					break;
				}
			}
		}
	}

	//Need to traverse indirect Block Pointer
	if(currentInode.indirectPointer != -1) {
	}
	//Need to traverse double indirect block pointer
	if(currentInode.doubleIndirectPointer != -1) {
	}

	cout << endl;
	
}

//COMPLETED LOGIC, BUT HAS BUGS
//Delete the given file and free all blocks associated with it
void _delete(string fileName) {

	//Create a local copy of the inode map
	fseek(fptr, super.inodeMapLocation, SEEK_SET);
	int tempInodeMap[256];
	fread(tempInodeMap, 256*sizeof(int), 1, fptr);

	//Create a local copy of the free block list
	fseek(fptr, super.freeBlocksLocation, SEEK_SET);
	bool tempFreeBlocks[super.numBlocks];
	fread(tempFreeBlocks, super.numBlocks*sizeof(bool), 1, fptr);


	//Loop through INode Map, if an inode is present at an index, traverse it
	//If the traversed iNode has the same name as desired fileName, save its index and exit the loop
	int iNodeInd = -1;
	inode currentInode;
	for(int i = 0; i < 32 && iNodeInd == -1; i++) {
		int tempBlock = tempInodeMap[i];
		if(tempBlock != -1) {
			fseek(fptr, super.blockSize * tempBlock, SEEK_SET);
			//NOT READING WHOLE BLOCK
			fread(&currentInode, sizeof(inode), 1, fptr);
			if(currentInode.fileName == fileName) {
				iNodeInd = i;
			}
		}
	}

	//If the file is not found, print an error message and quit
	if(iNodeInd == -1) {
		cerr << "Error: Cannot delete '" << fileName << "'. This file does not exist." << endl;
		return;
	}
	
	//Delete the direct pointers
	deleteData(currentInode, 12, currentInode.directPointers, tempFreeBlocks);

	//Delete the indirect pointer
	if(currentInode.indirectPointer != -1) {
		//Create an array of direct pointers, to be filled by the contents of the indirect pointer block
		int tempDirectPointers[super.blockSize/sizeof(int)];
		fseek(fptr, super.blockSize * currentInode.indirectPointer, SEEK_SET);
		fread(tempDirectPointers, super.blockSize, 1, fptr);

		//Call the deleteData function on this array
		deleteData(currentInode, super.blockSize/sizeof(int), tempDirectPointers, tempFreeBlocks);

		//Rewrite the new array to its original location, set the indirect Pointer to -1
		fseek(fptr, super.blockSize * currentInode.indirectPointer, SEEK_SET);
		fwrite(tempDirectPointers, super.blockSize, 1, fptr);
		currentInode.indirectPointer = -1;
		
	}
	
	//Delete the double indirect pointer
	if(currentInode.doubleIndirectPointer != -1) {
		//Create an array of indirect pointers, to be filled by the contents of the double indirect pointer block
		int tempIndirectPointers[super.blockSize/sizeof(int)];
		fseek(fptr, super.blockSize * currentInode.doubleIndirectPointer, SEEK_SET);
		fread(tempIndirectPointers, super.blockSize, 1, fptr);

		//Loop through this array, and use similar logic from the indirect pointer if statement for each indirect pointer in the array
		for(int i = 0; i < super.blockSize/sizeof(int); i++) {

			//Create an array of direct pointers, to be filled by the contents of the indirect pointer block
			int tempDirectPointers[super.blockSize/sizeof(int)];
			fseek(fptr, super.blockSize * tempIndirectPointers[i], SEEK_SET);
			fread(tempDirectPointers, super.blockSize, 1, fptr);

			//Call the deleteData function on this array
			deleteData(currentInode, super.blockSize/sizeof(int), tempDirectPointers, tempFreeBlocks);

			//Rewrite the new array to its original location, set the indirect Pointer to -1
			fseek(fptr, super.blockSize * tempIndirectPointers[i], SEEK_SET);
			fwrite(tempDirectPointers, super.blockSize, 1, fptr);
			tempIndirectPointers[i] = -1;	
		}

		//Rewrite the new array of indirect block pointers to its original location, set the d ouble indirect pointer to -1
		fseek(fptr, super.blockSize * currentInode.doubleIndirectPointer, SEEK_SET);
		fwrite(tempIndirectPointers, super.blockSize, 1, fptr);
		currentInode.doubleIndirectPointer = -1;
	}

	//Set the size of the inode and its name to default values
	currentInode.fileSize = 0;
	fill_n(currentInode.fileName, 33, '\0');

	//Go to that block and write the inode
	fseek(fptr, tempInodeMap[iNodeInd]*super.blockSize, SEEK_SET);
	fwrite(&currentInode, sizeof(inode), 1, fptr);
	fillBlockWithGarbage(fptr);

	//Rewrite inode map
	tempInodeMap[iNodeInd] = -1;
	fseek(fptr, super.inodeMapLocation, SEEK_SET);
	fwrite(tempInodeMap, 256*sizeof(int), 1, fptr);
	fillBlockWithGarbage(fptr);

	//Write back the updated free block list to the disk
	fseek(fptr, super.freeBlocksLocation, SEEK_SET);
	fwrite(tempFreeBlocks, super.numBlocks, 1, fptr);
	fillBlockWithGarbage(fptr);

}

//UNFINISHED
//IMPLEMENT ALL THREE CASES, NEED START
void _write(string fileName, char c, int start, int numBytes) {


	//Create a local copy of the inode map
	fseek(fptr, super.inodeMapLocation, SEEK_SET);
	int tempInodeMap[256];
	fread(tempInodeMap, 256*sizeof(int), 1, fptr);

	//Create a local copy of the free block list
	fseek(fptr, super.freeBlocksLocation, SEEK_SET);
	bool tempFreeBlocks[super.numBlocks];
	fread(tempFreeBlocks, super.numBlocks*sizeof(bool), 1, fptr);

	//Loop through INode Map, if an inode is present at an index, traverse it
	//If the traversed iNode has the same name as desired fileName, save its index and exit the loop
	int iNodeInd = -1;
	inode currentInode;
	for(int i = 0; i < 32 && iNodeInd == -1; i++) {
		int tempBlock = tempInodeMap[i];
		if(tempBlock != -1) {
			fseek(fptr, super.blockSize * tempBlock, SEEK_SET);
			//NOT READING WHOLE BLOCK
			fread(&currentInode, sizeof(inode), 1, fptr);
			if(currentInode.fileName == fileName) {
				iNodeInd = i;
			}
		}
	}
	//If the file is not found, print an error message and quit
	if(iNodeInd == -1) {
		fprintf(stderr, "File is not present in File System\n");
		return;
	}

	if(numBytes > super.maxFileSize) {
		fprintf(stderr, "Cannot write more than maximum file size.\n");
		return;
	}

	if(numBytes + currentInode.fileSize > super.maxFileSize) {
		fprintf(stderr, "Not enough space left in file.\n");
		return;
	}


	//CASE 1: Start is within range of fileSize; OVERWRITE
	if(start < currentInode.fileSize) {
		//CASE 1A: EXCLUSIVE OVERWRITE
		if(start + numBytes <= currentInode.fileSize) {
		}
		//CASE 1B: OVERWRITE + APPEND
		else {
		}
	}

	//CASE 2: Start is at fileSize; APPEND
	if(start == currentInode.fileSize) {
	}

	//CASE 3: Start is beyond fileSize; ERROR
	if(start > currentInode.fileSize) {
		fprintf(stderr, "Starting index is beyond size of file.\n");
		return;
	}
	




	writeData(currentInode, 12, numBytes, c, currentInode.directPointers, start);	

	//If there are still more bytes to read, need to check indirect block pointer
	if(numBytes != 0) {
		//Indirect block pointer does not exist, find a free block, create it, and fill it with -1s
		if(currentInode.indirectPointer == -1) {
			int freeBlock = getAFreeBlock();
			currentInode.indirectPointer = freeBlock;
			fseek(fptr, super.blockSize*freeBlock, SEEK_SET);
			int tempBlockPointers[super.blockSize/sizeof(int)];
			fill_n(tempBlockPointers, super.blockSize/sizeof(int), -1);
			fwrite(tempBlockPointers, sizeof(int), super.blockSize/sizeof(int), fptr);
		}
		//Now begin to write to the blocks
		fseek(fptr, super.blockSize*currentInode.indirectPointer, SEEK_SET);
		int indirectPointerTemp[super.blockSize/sizeof(int)];
		fread(indirectPointerTemp, sizeof(int), super.blockSize/sizeof(int), fptr);
		writeData(currentInode, super.blockSize/sizeof(int), numBytes, c, indirectPointerTemp, start);
	}
	//Rewrite inode
	fseek(fptr, super.blockSize * tempInodeMap[iNodeInd], SEEK_SET);
	fwrite(&currentInode, sizeof(currentInode), 1, fptr);
	fillBlockWithGarbage(fptr);
}

void _read(string fileName, int start, int numBytes) {
}

//COMPLETE
//List the names and sizes of all current inodes in the file
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
