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
#include <cstring>
#include <time.h>

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

using namespace std;

FILE* fptr;
char* buffer;

void _delete(string fileName);
void _write(string fileName, char c, int start, int numBytes);

//Reads numBlocksToRead blocks from the DISK, starting at byte number startLocation; puts the data into buffer
void readFullBlocks(int startLocation, string mode) {
	//Seek to the correct location in the disk
	fseek(fptr, startLocation, SEEK_SET);
	int numBlocksToRead;
	
	if(mode == "inodeMap") {
		//Calculate number of blocks the inodeMap list takes up
		numBlocksToRead = (256*sizeof(int))/super.blockSize;
	}
	
	else if (mode == "inode") {
		//Inode always takes up only one block
		numBlocksToRead = 1;
	}
	
	else if (mode == "freeBlockList") {
		//Calculate number of blocks the freeBlock list takes up
		//Could do just numBlocks/blockSize since each bool is 1 byte, but safer to use this way
		numBlocksToRead = (super.numBlocks/sizeof(bool))/super.blockSize;
	}
	
	else if (mode == "data") {
		//Data blocks are always individual, so just read one block
		numBlocksToRead = 1;
	}
	
	else {
		fprintf(stderr, "Wrong mode parameter given to readFullBlocks().\n");
		return;
	}
	
	//If numBlocksToRead is 0, we change it to 1 since we always have to read at least 1 full block
	if(numBlocksToRead == 0) numBlocksToRead = 1;
	//Multiply numBlocksToRead by blockSize to get the number of bytes to read, then read the data into the buffer array
	fread(buffer, super.blockSize*numBlocksToRead, 1, fptr);
}

int getAFreeBlock() {
	//Create a local copy of the free block list
	bool tempFreeBlocks[super.numBlocks];
	
	//Call function to read numBlocksToRead full blocks into the temporary char buffer we have created
	readFullBlocks(super.freeBlocksLocation, "freeBlockList");

	//Now that we have the data inside buffer, memcpy the data out of it into our temporary tempFreeBlocks array
	//The data will automatically be converted/read as bool values once put back into the bool tempFreeBlocks array
	//Read only as much data as we want to (the size of tempFreeBlocks)
	memcpy(&tempFreeBlocks, buffer, super.numBlocks*sizeof(bool));
	
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
	return tempInd;
}

int findBlock(inode& currentInode, int start) {
	//If start is within the direct blocks, return which index to go to
	return start/super.blockSize;

}
//Takes in an array which represents blocks, which could come from direct pointers, indirect pointers, or double indirect pointers
//Loops through this array and writes data to non-empty locations
void writeData(inode& currentInode, int loops, int& numBytes, char c, int* pointers, int& placeInBlock, int block, bool nullTerm) {
	//We take in an array of block indexes, which may be from direct or indirect blocks
	//Block is the block which start is in, so we start looping from there
	for(int i = block; i < loops && numBytes != 0; i++) {

		//If this direct pointer isn't free, find a free block for it
		if(pointers[i] == -1) {
			pointers[i] = getAFreeBlock();
			
			if(pointers[i] == -1) {
				/* currentInode.fileSize -= numBytes;
				
				//Rewrite inode
				fseek(fptr, super.blockSize * tempInodeMap[iNodeInd], SEEK_SET);
				fwrite(&currentInode, sizeof(currentInode), 1, fptr);
				fillBlockWithGarbage(fptr); */
				
				return;
			}
		}

		//For the first run of the loop, might need to start at some point other than the first byte
		if(i == block) {
			readFullBlocks(super.blockSize * pointers[i], "data");
			for(int j = placeInBlock; j < super.blockSize && numBytes != 0; j++) {
				buffer[j] = c;
				numBytes--;
				placeInBlock++;
			}

			//Append Null Terminator to the end of the buffer if there is room left
			if(placeInBlock != super.blockSize && !nullTerm) {
				buffer[placeInBlock] = '\0';
			}

			//Write this block back to the file
			fseek(fptr, super.blockSize * pointers[i], SEEK_SET);
			fwrite(buffer, super.blockSize, 1, fptr);
			
		}

		else {
			if(numBytes >= super.blockSize) {
				char arrayToWrite[super.blockSize];
				fill_n(arrayToWrite, super.blockSize, c);
				fseek(fptr, super.blockSize * pointers[i], SEEK_SET);
				fwrite(&arrayToWrite, super.blockSize, 1, fptr);
				numBytes -= super.blockSize;
			}

//			//Partially fill block with the char, then add a Null Terminator, then fill the rest with garbage and set numBytes to 0
			else{
				char arrayToWrite[numBytes+1];
				fill_n(arrayToWrite, numBytes, c);
				arrayToWrite[numBytes] = '\0';
				fseek(fptr, super.blockSize * pointers[i], SEEK_SET);
				fwrite(&arrayToWrite, numBytes+1, 1, fptr);
				
				//Record the specific byte after writing stopped, so we can resume writing later
				fillBlockWithGarbage(fptr);
				numBytes = 0;
			}
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
	int tempInodeMap[256];

	//Call function to read numBlocksToRead full blocks into the temporary char buffer we have created
	readFullBlocks(super.inodeMapLocation, "inodeMap");
	
	//Now that we have the data inside buffer, memcpy the data out of it into our temporary tempInodeMap array
	//The data will automatically be converted/read as int values once put back into the int tempInodeMap array
	//Read only as much data as we want to (the size of tempInodeMap)
	memcpy(&tempInodeMap, buffer, 256*sizeof(int));


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
			readFullBlocks(super.blockSize * tempBlock, "inode");
			memcpy(&currentInode, buffer, sizeof(inode));
			
			if(currentInode.fileName == fileName) {
				fprintf(stderr, "A File with the name %s already exists\n", fileName.c_str());
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

	//Call get a free block to get the next free block
	int nextFreeBlock = getAFreeBlock();
	//No free space left in disk to create a new inode, return
	if(nextFreeBlock == -1) {
		cerr << "Could not create this file; disk is currently full." << endl;
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

//DONE
void _import(string ssfsFile, string unixFile) {
	//Delete the existing file and then recreate it in order to overwrite contents easily
	_delete(ssfsFile);
	_create(ssfsFile);
	
	int length;
	char* unixBuffer;
	//Read contents of the inputFile into a char buffer, set the last character to be Null terminator
	ifstream inputFile(unixFile.c_str());
	if(inputFile.is_open()) {
		inputFile.seekg(0, inputFile.end);

		length = inputFile.tellg();
		unixBuffer = new char [length+1];
		inputFile.seekg(0, inputFile.beg);
		inputFile.read(unixBuffer, length);
		unixBuffer[length] = '\0';
		inputFile.close();
	}

	else {
		fprintf(stderr, "Unable to open the unixFile\n");
		return;
	}

	//Call write on each character
	for(int i= 0; i < length; i++) {
		_write(ssfsFile, unixBuffer[i], i, 1);
	}

	delete [] unixBuffer;
}

//ALMOST COMPLETED! CHECK LOGIC FOR DOUBLE INDIRECT BLOCK
void _cat(string fileName, int start, int numBytes) {
	//Create a local copy of the inode map	
	int tempInodeMap[256];
	readFullBlocks(super.inodeMapLocation, "inodeMap");
	memcpy(&tempInodeMap, buffer, 256*sizeof(int));

	//Loop through INode Map, if an inode is present at an index, traverse it
	//If the traversed iNode has the same name as desired fileName, save its index and exit the loop
	int iNodeInd = -1;
	inode currentInode;
	for(int i = 0; i < 256 && iNodeInd == -1; i++) {
		int tempBlock = tempInodeMap[i];
		if(tempBlock != -1) {
			readFullBlocks(super.blockSize * tempBlock, "inode");
			memcpy(&currentInode, buffer, sizeof(inode));
			
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

	int startingBlock = findBlock(currentInode, start);
	start %= super.blockSize;

	//Traverse through direct Pointers, stop reading if -1 is ever reached
	bool reading = true;
	for(int i = startingBlock; i < 12 && reading && numBytes != 0; i++) {
		if(currentInode.directPointers[i] == -1) {
			reading = false;
		}
		//Create a buffer, seek to each block and write each character to the buffer until the block is exhausted or a NULL terminator is found
		else {
			if(i == startingBlock) {
				readFullBlocks(super.blockSize*currentInode.directPointers[i], "data");
				for(int j = start; j < super.blockSize && numBytes != 0; j++) {
					if(buffer[j] == '\0') {
						break;
					}
					numBytes--;
					cout << buffer[j];
					start = 0;
				}	
			}
			else {
				readFullBlocks(super.blockSize * currentInode.directPointers[i], "data");		
				for(int j = 0; j < super.blockSize && numBytes != 0; j++) {
					if(buffer[j] == '\0') {
						break;
					}
					numBytes--;
					cout << buffer[j];
				}
			}
		}
	}
	//Decrement Starting Block by 12 if we are reading from previous, otherwise set it to 0
	if(startingBlock >= 12) {
		startingBlock -= 12;
	}
	else {
		startingBlock = 0;
	}

	//Need to traverse indirect Block Pointer; inside we use the same logic as before except instead of only 12 data block pointers we have blockSize/size int
	if(currentInode.indirectPointer != -1) {
		//Create an array of direct pointers, to be filled by the contents of the indirect pointer block
		int tempDirectPointers[super.blockSize/sizeof(int)];		
		readFullBlocks(super.blockSize * currentInode.indirectPointer, "data");
		memcpy(&tempDirectPointers, buffer, super.blockSize);

		//Traverse through direct Pointers stored in this indirectPointer block, stop reading if -1 is ever reached
		//Uses same logic as before
		bool reading = true;

		for(int i = startingBlock; i < super.blockSize/sizeof(int) && reading && numBytes != 0; i++) {
			if(tempDirectPointers[i] == -1) {
				reading = false;
			}
			//Create a buffer, seek to each block and write each character to the buffer until the block is exhausted or a NULL terminator is found
			else {
				int loop;
				if(i == startingBlock) {
					loop = start;
				}
				else {
					loop = 0;
				}
				readFullBlocks(super.blockSize * tempDirectPointers[i], "data");
				
				for(int j = loop; j < super.blockSize && numBytes != 0; j++) {
					if(buffer[j] == '\0') {
						break;
					}
					numBytes--;
					start = 0;
					cout << buffer[j];
				}
			}
		}
	}
	//Indirect start is the index of the indirect block to start reading from
	//startingBlock is the index of the block within the indirect block to start reading from
	int indirectStart;
	if(startingBlock >= super.blockSize/sizeof(int)) {
		startingBlock -= super.blockSize/sizeof(int);
		indirectStart = startingBlock / (super.blockSize/sizeof(int));
		startingBlock = startingBlock % (super.blockSize/sizeof(int));
	}
	else{
		//startingBlock = 0;
		indirectStart = 0;
		startingBlock = 0;
	}

	//Need to traverse double indirect block pointer
	bool reading2 = true;
	if(currentInode.doubleIndirectPointer != -1) {
		//Create an array of indirect pointers, to be filled by the contents of the double indirect pointer block
		int tempIndirectPointers[super.blockSize/sizeof(int)];
		readFullBlocks(super.blockSize * currentInode.doubleIndirectPointer, "data");
		memcpy(&tempIndirectPointers, buffer, super.blockSize);

		//Loop through this array, and use similar logic from the indirect pointer if statement for each indirect pointer in the array
		for(int i = indirectStart; i < super.blockSize/sizeof(int) && reading2 && numBytes != 0; i++) {

			//Create an array of direct pointers, to be filled by the contents of the indirect pointer block
			int tempDirectPointers[super.blockSize/sizeof(int)];		
			readFullBlocks(super.blockSize * tempIndirectPointers[i], "data");
			memcpy(&tempDirectPointers, buffer, super.blockSize);
		
			//Traverse through direct Pointers stored in this indirectPointer block, stop reading if -1 is ever reached
			//Uses same logic as before

			for(int j = startingBlock; j < super.blockSize/sizeof(int) && reading2 && numBytes != 0; j++) {
				if(tempDirectPointers[j] == -1) {
					reading2 = false;
				}

				//Create a buffer, seek to each block and write each character to the buffer until the block is exhausted or a NULL terminator is found
				else {
					int loop;
					if(j == startingBlock) {
						//Potential issue
						loop = start;
					}
					else {
						loop = 0;
					}
					readFullBlocks(super.blockSize * tempDirectPointers[j], "data");

					for(int k = loop; k < super.blockSize && numBytes != 0; k++) {
						if(buffer[k] == '\0') {
							reading2 = false;
							break;
						}
						numBytes--;
						start = 0;
						cout << buffer[k];
					}
				}
			}
		}
	}

	cout << endl;
	
}

//COMPLETED LOGIC, BUT HAS BUGS
//Delete the given file and free all blocks associated with it
void _delete(string fileName) {
	//Create a local copy of the inode map	
	int tempInodeMap[256];
	readFullBlocks(super.inodeMapLocation, "inodeMap");
	memcpy(&tempInodeMap, buffer, 256*sizeof(int));

	//Create a local copy of the free block list
	bool tempFreeBlocks[super.numBlocks];
	readFullBlocks(super.freeBlocksLocation, "freeBlockList");
	memcpy(&tempFreeBlocks, buffer, super.numBlocks*sizeof(bool));

	//Loop through INode Map, if an inode is present at an index, traverse it
	//If the traversed iNode has the same name as desired fileName, save its index and exit the loop
	int iNodeInd = -1;
	inode currentInode;
	for(int i = 0; i < 256 && iNodeInd == -1; i++) {
		int tempBlock = tempInodeMap[i];
		if(tempBlock != -1) {
			readFullBlocks(super.blockSize * tempBlock, "inode");
			memcpy(&currentInode, buffer, sizeof(inode));
			
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
		readFullBlocks(super.blockSize * currentInode.indirectPointer, "data");
		memcpy(&tempDirectPointers, buffer, super.blockSize);
		
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
		readFullBlocks(super.blockSize * currentInode.doubleIndirectPointer, "data");
		memcpy(&tempIndirectPointers, buffer, super.blockSize);

		bool reading = true;
		//Loop through this array, and use similar logic from the indirect pointer if statement for each indirect pointer in the array
		for(int i = 0; i < super.blockSize/sizeof(int) && reading; i++) {
			
			if(tempIndirectPointers[i] == -1) {
				reading = false;
			}

			else {
				//Create an array of direct pointers, to be filled by the contents of the indirect pointer block
				int tempDirectPointers[super.blockSize/sizeof(int)];			
				readFullBlocks(super.blockSize * tempIndirectPointers[i], "data");
				memcpy(&tempDirectPointers, buffer, super.blockSize);

				//Call the deleteData function on this array
				deleteData(currentInode, super.blockSize/sizeof(int), tempDirectPointers, tempFreeBlocks);

				//Rewrite the new array to its original location, set the indirect Pointer to -1
				fseek(fptr, super.blockSize * tempIndirectPointers[i], SEEK_SET);
				fwrite(tempDirectPointers, super.blockSize, 1, fptr);
				tempIndirectPointers[i] = -1;
			}
		}

		//Rewrite the new array of indirect block pointers to its original location, set the double indirect pointer to -1
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
	int tempInodeMap[256];
	readFullBlocks(super.inodeMapLocation, "inodeMap");
	memcpy(&tempInodeMap, buffer, 256*sizeof(int));

	//Create a local copy of the free block list
	bool tempFreeBlocks[super.numBlocks];
	readFullBlocks(super.freeBlocksLocation, "freeBlockList");
	memcpy(&tempFreeBlocks, buffer, super.numBlocks*sizeof(bool));

	//Loop through INode Map, if an inode is present at an index, traverse it
	//If the traversed iNode has the same name as desired fileName, save its index and exit the loop
	int iNodeInd = -1;
	inode currentInode;
	for(int i = 0; i < 256 && iNodeInd == -1; i++) {
		int tempBlock = tempInodeMap[i];
		if(tempBlock != -1) {
			readFullBlocks(super.blockSize * tempBlock, "inode");
			memcpy(&currentInode, buffer, sizeof(inode));
			
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

	//Start is beyond fileSize; ERROR
	if(start > currentInode.fileSize) {
		fprintf(stderr, "Starting index is beyond size of file.\n");
		return;
	}
	
	int blockToWrite = findBlock(currentInode, start);
	int placeInBlock = start%super.blockSize;

	bool nullTerm = false;

	//We write fileSize here since file will eventually be of file size = currentInode.fileSize + numBytes; only set the value once
	//Previously we were adding numBytes to fileSize each time in writeData, so it was giving wrong value for fileSize
	if(start + numBytes >= currentInode.fileSize) {
		currentInode.fileSize = start + numBytes;
		
		//Need to do this
		/*
		if((start + numBytes) > some variable that gives maximum number of bytes we can still write for this file) {
			currentInode.fileSize = max size that we will be able to write;
		}
		
		else {
			currentInode.fileSize = start + numBytes;
		}
		
		*/
	} else {
		nullTerm = true;
	}
	
	//Write into first 12 data blocks first
	writeData(currentInode, 12, numBytes, c, currentInode.directPointers, placeInBlock, blockToWrite, nullTerm);
	
	//If there are still more bytes to read, need to check indirect block pointer
	if(numBytes != 0) {
		//Indirect block pointer does not exist, find a free block, create it, and fill it with -1s
		if(currentInode.indirectPointer == -1) {
			int freeBlock = getAFreeBlock();
			
			if(freeBlock == -1) {
				currentInode.fileSize -= numBytes;
				
				//Rewrite inode
				fseek(fptr, super.blockSize * tempInodeMap[iNodeInd], SEEK_SET);
				fwrite(&currentInode, sizeof(currentInode), 1, fptr);
				fillBlockWithGarbage(fptr);
				
				cout << "Ran out of space in disk, wrote maximum amount possible to file." << endl;
					
				return;
			}
			
			currentInode.indirectPointer = freeBlock;
			fseek(fptr, super.blockSize*freeBlock, SEEK_SET);
			int tempBlockPointers[super.blockSize/sizeof(int)];
			fill_n(tempBlockPointers, super.blockSize/sizeof(int), -1);
			fwrite(tempBlockPointers, super.blockSize, 1, fptr);
		}
		
		//Now begin to write to the blocks
		fseek(fptr, super.blockSize*currentInode.indirectPointer, SEEK_SET);
		int indirectPointerTemp[super.blockSize/sizeof(int)];
		fread(indirectPointerTemp, sizeof(int), super.blockSize/sizeof(int), fptr);
		
		//This scenario is for if we start writing to the indirect block from the start
		if(blockToWrite >= 12) {
			blockToWrite -= 12;
		}
		//This scenario is for if we wrote to the direct block but the rest needs to go to the indirect block
		else {
			placeInBlock = 0;
			blockToWrite = 0;
		}
		writeData(currentInode, super.blockSize/sizeof(int), numBytes, c, indirectPointerTemp, placeInBlock, blockToWrite, nullTerm);
		
		//After writeData returns, the temporary indirectPointerTemp array we created has all the updated pointers to the new data blocks
		//Need to write this array back to the disk at block super.blockSize*currentInode.indirectPointer since that's where this array will be stored
		//For the first 12 data block pointers, we were already updating the array inside the inode, so didn't have to write back to disk manually
		fseek(fptr, super.blockSize*currentInode.indirectPointer, SEEK_SET);
		fwrite(indirectPointerTemp, super.blockSize, 1, fptr);
		
	}

	//---------------------------------------------------------------------------------------------------------
	//Now need to check if even more numBytes left, for doubleIndirectPointer
	if(numBytes != 0) {
		//This is an array of indirect Block pointers
		int indirectBlockPointers[super.blockSize/sizeof(int)];
	
		//Double Indirect block pointer does not exist, find a free block, create it, and fill it with -1s
		if(currentInode.doubleIndirectPointer == -1) {
			//allocate free space for the double indirect block pointer
			int freeBlock = getAFreeBlock();
			
			if(freeBlock == -1) {
				currentInode.fileSize -= numBytes;
				
				//Rewrite inode
				fseek(fptr, super.blockSize * tempInodeMap[iNodeInd], SEEK_SET);
				fwrite(&currentInode, sizeof(currentInode), 1, fptr);
				fillBlockWithGarbage(fptr);
				
				cout << "Ran out of space in disk, wrote maximum amount possible to file." << endl;
				
				return;
			}
			
			currentInode.doubleIndirectPointer = freeBlock;
			//Write the array of indirect block pointers to the location of the double indirect block pointer
			fseek(fptr, super.blockSize*freeBlock, SEEK_SET);
			fill_n(indirectBlockPointers, super.blockSize/sizeof(int), -1);
			fwrite(indirectBlockPointers, super.blockSize, 1, fptr);
		}
		
		//Now begin to write to the blocks
		fseek(fptr, super.blockSize*currentInode.doubleIndirectPointer, SEEK_SET);
		fread(indirectBlockPointers, super.blockSize, 1, fptr);

		//Loop through the elements in the array of indirect block pointers, create arrays of block pointers in each one and write them
		for(int i = 0; i < super.blockSize/sizeof(int) && numBytes != 0; i++) {
			//This is an array of direct Block pointers
			int blockPointers[super.blockSize/sizeof(int)];
		
			//Set the index of the indirect block pointer to be to a block of direct pointers
			if(indirectBlockPointers[i] == -1) {
				int freeBlock = getAFreeBlock();
				
				if(freeBlock == -1) {
					currentInode.fileSize -= numBytes;
					
					//Write the block of indirect pointers
					fseek(fptr, super.blockSize*currentInode.doubleIndirectPointer, SEEK_SET);
					fwrite(indirectBlockPointers, super.blockSize, 1, fptr);
					
					//Rewrite inode
					fseek(fptr, super.blockSize * tempInodeMap[iNodeInd], SEEK_SET);
					fwrite(&currentInode, sizeof(currentInode), 1, fptr);
					fillBlockWithGarbage(fptr);
					
					cout << "Ran out of space in disk, wrote maximum amount possible to file." << endl;
					
					return;
				}
				
				indirectBlockPointers[i] = freeBlock;
				fseek(fptr, super.blockSize*freeBlock, SEEK_SET);
				fill_n(blockPointers, super.blockSize/sizeof(int), -1);
				fwrite(blockPointers, super.blockSize, 1, fptr);
			}

			//Now begin to write to the blocks
			fseek(fptr, super.blockSize*indirectBlockPointers[i], SEEK_SET);
			fread(blockPointers, super.blockSize, 1, fptr);

			//Check where the block is, if it's in an indirect block allocated from a double indirect block, subtract to get the indexes
			//This is for if the starting index is in the double indirect pointer
			if(blockToWrite >= super.blockSize/sizeof(int)) {
				blockToWrite -= super.blockSize/sizeof(int);
			}
			//this is for if we did some previous writing and needed more space
			else {
				placeInBlock = 0;
				blockToWrite = 0;
			}

			writeData(currentInode, super.blockSize/sizeof(int), numBytes, c, blockPointers, placeInBlock, blockToWrite, nullTerm);
			
			fseek(fptr, super.blockSize*indirectBlockPointers[i], SEEK_SET);
			fwrite(blockPointers, super.blockSize, 1, fptr);
		}

		//Write the block of indirect pointers
		fseek(fptr, super.blockSize*currentInode.doubleIndirectPointer, SEEK_SET);
		fwrite(indirectBlockPointers, super.blockSize, 1, fptr);

	}
	
	//Rewrite inode
	fseek(fptr, super.blockSize * tempInodeMap[iNodeInd], SEEK_SET);
	fwrite(&currentInode, sizeof(currentInode), 1, fptr);
	fillBlockWithGarbage(fptr);
}

//COMPLETE
//List the names and sizes of all current inodes in the file
void _list() {
	//Create a local copy of the inode map
	int tempInodeMap[256];
	readFullBlocks(super.inodeMapLocation, "inodeMap");
	memcpy(&tempInodeMap, buffer, 256*sizeof(int));
	bool foundAFile = false;

	//Loop through the iNode map, if the pointer is not to -1 then an inode exists
	for(int i = 0; i < 256; i++) {
		if(tempInodeMap[i] != -1) {
			//Seek to block where the inode is held, print out its name and size
			inode currentInode;
			readFullBlocks(super.blockSize * tempInodeMap[i], "inode");
			memcpy(&currentInode, buffer, sizeof(inode));
			
			cout << "File Name: " << currentInode.fileName << "\tFile Size: " << currentInode.fileSize << endl;
			
			foundAFile = true;
		}
	}
	
	if (foundAFile == false) {
		cout << "No files currently exist." << endl;
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

	//Open the disk
	fptr = fopen(diskFileName.c_str(), "rb+");
	fread(&super, sizeof(super), 1, fptr);

	//Create a temporary char buffer which will store all the data when we're reading out of DISK for the rest of the program
	buffer = new char[super.numBlocks*super.blockSize];

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
			_cat(command, 0, -1);
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
			_cat(fileName, start, numBytes);
		}
		else if(command == "LIST") {
			_list();
		}
		else if(command == "SHUTDOWN") {
			cout << "Shutting down." << endl;
			break;
		}		
		else if(command == "DOG") {
			string names[4];
			names[0] = "arf";
			names[1] = "bark";
			names[2] = "woof";
			names[3] = "bork";

			srand(time(NULL));
			int r = rand() % 4;
			cout << names[r] << endl;
		}
		else if(command == "FOX") {
			cout << "The year is 20XX. Everyone plays Fox at TAS levels of perfection. Because of this, the winner of a match depends solely on port priority. The RPS metagame has evolved to ridiculous levels due to it being the only remaining factor to decide matches" << endl;
		}
		

	}
	thread0.close();
	
	//cerr << super.maxFileSize << endl;
}
