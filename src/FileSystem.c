/**
 * Filesytem stores a superblock at the first two blocks of the volume. The second
 * block is a copy of the first for redunancy. Inodes are kept in the next portion of the
 * filesystem. Then the freeblocks bitvectors are stored after the inodes. Finally the root pointer
 * to the first directory that links to other directories, files, and data.
 */

#include <string.h>
#include <stdbool.h>
#include "FileSystem.h"

extern int errno;

SuperBlock_p sb = NULL;
uint8_t * bitVector = NULL;
WorkingDirectory_p wd = NULL;


/**
 * Finds the next closest prime number to the given minimum
 * block size. This allows for the hash function to be more
 * efficient.
 * Returns the next closest Prime number to the given minBlockSize
 */
uint64_t findNextPrime(uint64_t minBlockSize) {
	bool prime = true;
	bool incrementByTwo = false;

	if (minBlockSize <= 3)
		return minBlockSize;

	uint64_t start = minBlockSize % 6;
	if (start == 0)
		minBlockSize++;
	else if (start > 1 && start < 5) {
		minBlockSize += (5 - start);
		incrementByTwo = true;
	}

	while (true) {
		if (!prime) {
			if (incrementByTwo) {
				minBlockSize += 2;
				incrementByTwo = false;
			} else {
				minBlockSize += 4;
				incrementByTwo = true;
			}
			prime = true;
		}

		if (minBlockSize % 2 == 0) {
			prime = false;
			continue;
		} else if (minBlockSize % 3 == 0) {
			prime = false;
			continue;
		}

		uint64_t i = 5;
		while (i + 2 < minBlockSize) {
			if (minBlockSize % i == 0) {
				prime = false;
				break;
			} else if (minBlockSize % (i + 2) == 0) {
				prime = false;
				break;
			}
			i += 6;
		}
		if (prime)
			break;
	}
	return minBlockSize;
}

/**
 * Hashes the name of the file/folder along with the parent
 * inode and should return an unused inode
 * Returns the hash
 */
uint64_t hashInode(char* name, uint64_t parentInode) {
	uint64_t hash = 7243;

	while (*name) {
		hash += (hash << 5) + *name + parentInode;
		name++;
	}
	return hash % sb->numInodes;
}

/**
 * Finds and returns a free inode number in the inode table.
 * Returns 0 if unsuccessful
 * Returns a free inode
 */
uint64_t findFreeInode(char* name, uint64_t parentInode) {
	if (sb->usedInodes == sb->numInodes)
		return 0;

	uint64_t numberSearched = 0;
	uint64_t inodeID = hashInode(name, parentInode);
	uint64_t currentID = inodeID;

	char* buffer = malloc(partInfop->blocksize * 2);
	uint64_t byteLocation = inodeID * sizeof(Inode) + partInfop->blocksize * sb->inodeStart;
	uint64_t blockLocation = byteLocation / partInfop->blocksize;
	uint64_t offset = byteLocation % partInfop->blocksize;
	LBAread(&buffer[partInfop->blocksize], 1, blockLocation);
	blockLocation++;
	while (numberSearched < sb->numInodes - 1) {
		memcpy(buffer, &buffer[partInfop->blocksize], partInfop->blocksize);
		if (blockLocation < sb->freeBlocks) {
			LBAread(&buffer[partInfop->blocksize], 1, blockLocation);
		}

		while (offset < partInfop->blocksize * 2 - sizeof(Inode)) {
			if (buffer[offset] == UNUSED_FLAG) {
				free(buffer);
				return currentID;
			} else {
				numberSearched++;
				currentID++;
				if (currentID > sb->numInodes) {
					currentID = 1;
					offset = sizeof(Inode);
					break;
				} else {
					offset += sizeof(Inode);
				}
			}
		}
		if (currentID == 1) {
			blockLocation = sb->inodeStart;
		} else {
			blockLocation++;
			offset -= partInfop->blocksize;
		}
	}
	free(buffer);
	return 0;
}

/**
 * Reads the data of the file from the filesystem and stores it into the destination.
 * If the pointer is null, then memory will be allocated to hold the file data.
 * @param destination the buffer that the file data will be stored in.
 * @param inodeID the file's inode.
 * @param length is the number of bytes to read. 0 if reading the entire file.
 * Returns the number of bytes read into destination if successful
 * Returns 0 if unsuccessful
 */
uint64_t readFile(char* destination, const uint64_t inodeID, const uint64_t length) {
	Inode_p inodeBuffer = malloc(sizeof(Inode));
	uint64_t bytesToRead;
	uint64_t numberOfBlocksToRead;
	uint64_t blockToRead;
	char* blockBuffer = malloc(partInfop->blocksize);
	memset(blockBuffer, 0, partInfop->blocksize);
	char* indirectBlockBuffer = malloc(partInfop->blocksize * NUM_INDIRECT);

	if (readInode(inodeID, inodeBuffer) == -1) {
		printf("Error: Failed retrieving inode %lu", inodeID);
		return 0;
	}

	if (length == 0)
		bytesToRead = inodeBuffer->size;
	else
		bytesToRead = length;

	if (destination == NULL)
		destination = malloc(bytesToRead);

	numberOfBlocksToRead = (bytesToRead + partInfop->blocksize - 1) / partInfop->blocksize;
	uint32_t j = 0;
	for (uint32_t i = 0; i < numberOfBlocksToRead; i++) {
		if (i < NUM_DIRECT) {
			blockToRead = sb->rootDataPointer + inodeBuffer->directData[i];
		} else if (i < NUM_DIRECT + sb->maxPointersPerIndirect[0]) {
			if (i == NUM_DIRECT)
				LBAread(indirectBlockBuffer, 1, inodeBuffer->indirectData[0]);

			blockToRead = indirectBlockBuffer[i-NUM_DIRECT] + sb->rootDataPointer;
		} else {
			if (i == NUM_DIRECT + sb->maxPointersPerIndirect[0])
				LBAread(&indirectBlockBuffer[partInfop->blocksize], 1, inodeBuffer->indirectData[1] + sb->rootDataPointer);

			if ((i - (NUM_DIRECT + sb->maxPointersPerIndirect[0])) % sb->maxPointersPerIndirect[0] == 0) {
				LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[partInfop->blocksize + j] + sb->rootDataPointer);
				j++;
				if (j > sb->maxPointersPerIndirect[0] - 1) {
					printf("Error: This filesystem does not support this large of a file size");
					return 0;
				}
			}
			blockToRead = indirectBlockBuffer[((i - NUM_DIRECT) % sb->maxPointersPerIndirect[0])] + sb->rootDataPointer;
		}
		if (i == numberOfBlocksToRead - 1) {
			LBAread(blockBuffer, 1, blockToRead);
			memcpy(&destination[i], blockBuffer, (bytesToRead % partInfop->blocksize));
		} else {
			LBAread(&destination[i], 1, blockToRead);
		}
	}
	return bytesToRead;
}

/**
 * Given an inode number and an Inode_p pointer, readInode
 * will read from the request inode into either a buffer already
 * preallocated, or will allocate a buffer if the pointer is null.
 * Returns 0 if successful
 * Returns -1 if unsuccessful
 */
int readInode(uint64_t inodeID, Inode_p inodeBuffer) {
	if (inodeID >= sb->numInodes || inodeID < 0)
		return -1;

	if (inodeBuffer == NULL) {
		inodeBuffer = malloc(sizeof(Inode));
		memset(inodeBuffer, 0, sizeof(Inode));
	}

	char* buffer = malloc(partInfop->blocksize * 2);
	memset(buffer, 0, partInfop->blocksize * 2);
	uint64_t byteLocation = inodeID * sizeof(Inode) + partInfop->blocksize * sb->inodeStart;
	uint64_t blockLocation = byteLocation / partInfop->blocksize;
	uint64_t offset = byteLocation % partInfop->blocksize;
	if (offset > partInfop->blocksize - sizeof(Inode))
		LBAread(buffer, 2, blockLocation);
	else
		LBAread(buffer, 1, blockLocation);
	memcpy(inodeBuffer, &buffer[offset], sizeof(Inode));
	free(buffer);
	return 0;
}

/**
 * Given an inode number and an Inode_p buffer, writeInode
 * will write to the given inode number from the provided buffer.
 * The inode buffer must be preallocated with the contents.
 * Returns 0 if successful
 * Returns -1 if unsuccessful
 */
int writeInode(uint64_t inodeID, Inode_p inodeBuffer) {
	if (inodeBuffer == NULL || inodeID >= sb->numInodes || inodeID < 0)
		return -1;

	char* buffer = malloc(partInfop->blocksize * 2);
	memset(buffer, 0, partInfop->blocksize * 2);
	uint64_t byteLocation = inodeID * sizeof(Inode) + partInfop->blocksize * sb->inodeStart;
	uint64_t blockLocation = byteLocation / partInfop->blocksize;
	uint64_t offset = byteLocation % partInfop->blocksize;
	if (offset > partInfop->blocksize - sizeof(Inode)) {
		LBAread(buffer, 2, blockLocation);
		memcpy(&buffer[offset], inodeBuffer, sizeof(Inode));
		LBAwrite(buffer, 2, blockLocation);
	} else {
		LBAread(buffer, 1, blockLocation);
		memcpy(&buffer[offset], inodeBuffer, sizeof(Inode));
		LBAwrite(buffer, 1, blockLocation);
	}
	free(buffer);
	return 0;
}

/**
 * Checks for the filesystem on this partition, by checking the signatures of the first block for a match.
 * Returns returns 1 if filesystem exists
 * Returns 0 if filesystem does not exist.
 */

void initWorkingDirectory() {
    WorkingDirectory_p wdBuffer = malloc(sizeof(WorkingDirectory));
    wdBuffer->directoryInode= malloc(sizeof(Inode));

    
    wd = malloc(sizeof(WorkingDirectory));
    wd->directoryInode = malloc(sizeof(Inode));
    wd->numberOfDirectories = 0;
  
    strcpy(wd->pathName,"/\n");
    readInode(0,wd->directoryInode);
    Inode_p buff = wd->directoryInode;

    printf("Inode: %d\n", buff->inode);
    printf("Path: %s\n", wd->pathName);
    printf("Directories: %d\n",wd->numberOfDirectories);

    
    
}
int check_fs() {
	SuperBlock_p buffer = malloc(partInfop->blocksize);
	LBAread(buffer, 1, 0);
	if (buffer->superSignature != SUPER_SIGNATURE || buffer->superSignature2 != SUPER_SIGNATURE2) {
		free(buffer);
		return 0;
	}
	sb = malloc(sizeof(SuperBlock));
	memcpy(sb, buffer, sizeof(SuperBlock));
	free(buffer);
	return 1;
}

/**
 * Formats the current partition and installs a new filesystem.
 * returns 0 if format was successful
 * returns -1 if format was unsuccessful
 */
int fs_format() {
	SuperBlock_p buffer = malloc(partInfop->blocksize);
	memset(buffer, 0, partInfop->blocksize);
	buffer->superSignature = SUPER_SIGNATURE;
	/* For every BLOCKS_PER_INODE there is one Inode. Set to a prime number to make the hash more efficient */
	buffer->numInodes = findNextPrime(partInfop->numberOfBlocks / BLOCKS_PER_INODE);
	buffer->inodeStart = 1;	//Inode blocks start right after superblock

	/* Free Blocks starts at one block after blocks used by inodes and block used by superblock */
	buffer->bitVectorStart = ((buffer->numInodes * sizeof(Inode) + partInfop->blocksize - 1) / partInfop->blocksize) + buffer->inodeStart;
	/* Total Free Blocks = Total number of blocks - total blocks used by inodes - block used by superblock - blocks used by freeblocks */
	uint64_t unusedBlocks = partInfop->numberOfBlocks - (buffer->bitVectorStart - 1);
	uint64_t totalBytesForBitVector = (unusedBlocks * partInfop->blocksize + 8 - 1) / 8;
	uint64_t blocksUsedByBitVector = (totalBytesForBitVector + partInfop->blocksize - 1) / partInfop->blocksize;
	buffer->freeBlocks = unusedBlocks - blocksUsedByBitVector;
	buffer->usedBlocks = 0;
	buffer->totalDataBlocks = buffer->freeBlocks;
	buffer->maxPointersPerIndirect[0] = sizeof(uint64_t) / partInfop->blocksize;
	for (uint32_t i = 1; i < NUM_INDIRECT; i++) {
		uint64_t pointers = buffer->maxPointersPerIndirect[0];
		for (uint32_t j = 0; j < i; j++)
			pointers *= buffer->maxPointersPerIndirect[0];
		buffer->maxPointersPerIndirect[i] = pointers;
	}
	buffer->rootDataPointer = buffer->bitVectorStart + blocksUsedByBitVector;
	buffer->superSignature2 = SUPER_SIGNATURE2;

	if (LBAwrite(buffer, 1, 0) == 0) {
		free(buffer);
		return -1;
	}
	sb = malloc(sizeof(SuperBlock));
	memcpy(sb, buffer, sizeof(SuperBlock));
	free(buffer);

	/* Initialize Inodes */
	uint64_t inodeReservedBlocks = sb->bitVectorStart - sb->inodeStart;
	Inode_p inode_buffer = malloc(partInfop->blocksize * inodeReservedBlocks);
	memset(inode_buffer, 0, partInfop->blocksize * inodeReservedBlocks);
	if (LBAwrite(inode_buffer, inodeReservedBlocks, sb->inodeStart) == 0) {
		free(inode_buffer);
		return -1;
	}
	free(inode_buffer);

	/* Initialize bit vector */
	char* bvBuffer = malloc(blocksUsedByBitVector * partInfop->blocksize);
	memset(bvBuffer, 0, blocksUsedByBitVector * partInfop->blocksize);
	if (LBAwrite(bvBuffer, blocksUsedByBitVector, sb->bitVectorStart) == 0) {
		free(bvBuffer);
		return -1;
	}
    
        
    Inode_p root = calloc(1, sizeof(Inode));
    root->used = USED_FLAG;
    root->type = DIRECTORY_TYPE;
    root->inode = 0;
    root->dateModified = time(NULL);
    root->parent_p = 0;
    root->directData[0] = 0;
    root->size = 0;
    root->blocksReserved = 1;
    setBitOn(0);
    writeInode(0, root);
    Inode_p buff = calloc(1,sizeof(Inode));
    readInode(0,buff);
    
    readBitVector();
    writeBitVector();
    initWorkingDirectory();
    
    
	free(bvBuffer);
	return 0;
}

/** Outputs data about the current filesystem */
void fs_lsfs() {
	printf("Volume name: %s\n", partInfop->volumeName);
	printf("Volume size: %ld\n", partInfop->volumesize);
	printf("Block size: %ld\n", partInfop->blocksize);
	printf("Blocks: %ld\n", partInfop->numberOfBlocks);
	printf("Inodes: %ld\n", sb->numInodes);
	printf("Free Blocks: %ld\n", sb->freeBlocks);
	printf("Used Blocks: %ld\n", sb->usedBlocks);
	printf("Inode index: %ld\n", sb->inodeStart);
	printf("Bit Vector index: %ld\n", sb->bitVectorStart);
	printf("Root index: %ld\n", sb->rootDataPointer);
}

/** Lists the files in the current directory */
void fs_ls() {


}

/**
 * Creates a directory of the given name.
 * returns 0 if sucessful
 * returns -1 if it could not create directory
 * returns -2 if directory already exists
 */
int fs_mkdir(char* directoryName) {
    
     //parse directory name
           int count=0;
           char dirName[128];
           while(*directoryName!= '\0' ){
           
           dirName[count]=*directoryName;
               count++;
               directoryName++;
        
           }
           printf("%s",dirName);
           printf("%d",count);
       
       
           
           int targetDirectory=0;
        
           uint64_t directOffset=-1;
           uint64_t indirectFirstOffset=-1;
           uint64_t indirectSecondOffset=-1;
           uint64_t * indirectPointer=malloc(sizeof(uint64_t));
           uint64_t inodeIndex=0;
           uint64_t * indirectBlock = malloc(partInfop->blocksize);
           memset(indirectBlock,0,partInfop->blocksize);
       //Buffers for traversing directory tree
           char* nameBuffer = malloc(sizeof(char));
           nameBuffer=directoryName;
           Inode_p inodeBuffer=malloc(sizeof(Inode));
           memset(inodeBuffer, 0, sizeof(Inode));
           FCB_p buffer= malloc(sizeof(FCB));
           memset(buffer, 0, sizeof(FCB));
           char * blockBuffer = malloc(partInfop->blocksize);
           memset(blockBuffer, 0, partInfop->blocksize);
           
           
        /*
           while(targetDirectory==0){
           while(*nameBuffer!='/'|| *nameBuffer!='\0'){
               if(*nameBuffer=='\0'){
                   targetDirectory=1;
               }
               //strcat(dirName,directoryName);
               //nameBuffer++;
           }*/
       
       //get the directory inode from working directory
       Inode_p wdInode= wd->directoryInode;

       readInode(wdInode->inode,inodeBuffer);

       
       
       //Travesre through all direct data pointers
       for(int k = 0;k<NUM_DIRECT;k++){
          
           //Read the block that the direct data pointer points to
           LBAread(blockBuffer,1,inodeBuffer->directData[k]);
           
           //Loop through the file control blocks
           for(int l=0;l<partInfop->blocksize/sizeof(FCB);l++){
               
               //set buffer to FBC in block buffer
               buffer=blockBuffer+sizeof(FCB)*l;
               printf("(%s , %s)",buffer->name, dirName);
           
           //compare name of FCB name to name of directory trying to be made
           if(strncmp(dirName,buffer->name,count)==0 && targetDirectory==0){
               
               //if directory already exisits return -2
               return -2;
               
               /*inodeIndex=buffer->inodeID;
               directOffset=l;
               
               printf("%s", "*");*/
               
           }else{
               
               FCB_p newFCB=malloc(sizeof(FCB));
               
               //set buffer FCB to avaiable Inode
               newFCB->inodeID=findFreeInode(dirName,inodeBuffer->inode);
               
               //set name of FCB buffer to directory name
               strcpy(newFCB->name,dirName);
               
               //copy FCB buffer to memory
               memcpy(blockBuffer+sizeof(FCB)*(l+1),newFCB,sizeof(FCB));
               
               //Write buffer to available slot in directData
               LBAwrite(blockBuffer,1,inodeBuffer->directData[k]);
               
               
           }
          /* if(strncmp(buffer->name,dirName,count)==0 && targetDirectory==1){
               return -2;
           }*/
           }
       }
       
           
       
       
       
           
               
           if(inodeIndex==0){
               for(int n=0;n<NUM_INDIRECT;n++){
                   indirectPointer=inodeBuffer->indirectData[n];
     
                   LBAread(indirectBlock,1,indirectPointer);
                   indirectPointer=*indirectBlock;
                   //printf("n%d",indirectPointer);
                   
                   for(int m=0;m<(partInfop->blocksize/sizeof(uint64_t));m++){
                       LBAread(indirectBlock,1,indirectPointer);
                       indirectPointer=*indirectBlock+sizeof(uint64_t)*m;
                       LBAread(indirectBlock,1,indirectPointer);

                       for(int l=0;l<partInfop->blocksize/sizeof(FCB);l++){
                  
                       
                           buffer=indirectBlock+sizeof(FCB)*l;
                 
                   
                       if(strncmp(dirName,buffer->name,6)==0){
                     
                           inodeIndex=buffer->inodeID;
                           directOffset=l;
                   
                       }
                       if(strncmp(buffer->name,dirName,128) && targetDirectory==1){
                           return -2;
                       }
                       }
                           if(strncmp(buffer->name,dirName,128) && targetDirectory==1){
                               return 2;
                           }
                           buffer++;
                       }
                   }
               }
           if(inodeIndex==0){
               return -1;
           }
                   
              

           
        /*   if(indirectFirstOffset>0){
           
           readInode(inodeIndex,inodeBuffer);
           indirectPointer=inodeBuffer->directData[indirectFirstOffset]+sb->rootDataPointer;
               buffer=indirectPointer+indirectSecondOffset*sizeof(FCB);
           buffer+directOffset;
           strcpy((buffer->name),dirName);
           buffer->inodeID=inodeIndex;
                              }*/

}

/**
 * Deletes the directory with the given name.
 * returns 0 if successful
 * returns -1 if it could not delete directory
 * returns -2 if directory does not exist
 */
int fs_rmdir(char* directoryName) {

}

/**
 * Copies the file from source to destination
 * returns 0 if successful
 * returns -1 if could not copy file
 * returns -2 if source file does not exist
 */
int fs_cp(char* sourceFile, char* destFile) {
		int srcfd, desfd;
		char * buf;
		srcfd = myfsOpen(sourceFile);
		desfd = myfsOpen(destFile);
		//read entire file
		readFile(buf,openFileList[srcfd].inodeId,0);
		//write to destination file
		writeFile(openFileList[desfd].inodeId, buf, 0);
		myfsClose(srcfd);
		myfsClose(desfd);
}

/**
 * Moves the file from source to destination
 * returns 0 if successful
 * returns -1 if could not move file
 * returns -2 if source file does not exist
 */
int fs_mv(char* sourceFile, char* destFile) {

}

/**
 * Deletes the file with the given name
 * returns 0 if successful
 * returns -1 if could not delete file
 * returns -2 if file does not exist
 */
int fs_del(char* filename) {

}

/**
 * Copies a file from another filesystem to destination
 * in current filesystem.
 * returns 0 if successful
 * returns -1 if unsuccessful
 * returns -2 if source file does not exist
 */
int fs_cpin(char* sourceFile, char* destFile)
{
	int srcfd, desfd;

	srcfd = open(sourceFile, O_RDONLY);//open source file in linux for read only

	off_t srcSize = lseek(srcfd, 0, SEEK_END);

	char *buf = malloc(srcSize);

	desfd = myfsOpen(destFile); //open destination file

	read(srcfd,buf,srcSize);
	writeFile(openFileList[desfd].inodeId, buf, 0);
	close(srcfd);
	myfsClose(desfd);
}

/**
 * Copies a file from this filesystem to Linux
 * returns 0 if successful
 * returns -1 if unsuccessful
 * returns -2 if source file does not exist
 */
int fs_cpout(char* sourceFile, char* destFile)
{
	int srcfd, desfd;
	char * buf;
	srcfd = myfsOpen(sourceFile); //open file for read
	desfd = open(destFile, O_WRONLY|O_CREAT);  //create destination file in linux for write
	//error checking in Linux file system
	if(desfd == -1)
	{
		printf("Error Number % d\n", errno);
		perror("Program");
	}
	//read then write
	readFile(buf, openFileList[srcfd].inodeId, 0);
		//myfsSeek(srcfd,partInfop->blocksize,MYSEEK_CUR);
	write(desfd,buf, openFileList[srcfd].size);
	myfsClose(srcfd);
	close(desfd);
}

//similar to fsOpen in Linux, based off Professor Bierman's demo in class
int myfsOpen(char *filename)
{
	int fd;
	//get a file descriptor
	for(int i = 0; i < FDOPENMAX; i++)
	{
		if(openFileList[i].flags == FDOPENFREE)
		{
			fd = i;
			break;
		}

	//find file in directory
	


	//null, file does not exist
	openFileList[i].flags = FDOPENINUSE|FDOPENFORREAD|FDOPENFORWRITE;
	openFileList[i].filebuffer = malloc (partInfop->blocksize *2); //allocate 2 blocks for the file
	openFileList[i].position = 0; //seek is beginning of FILEIDINCREMENT
	openFileList[i].size  = 0; //assume it's empty file (this is from demo in class)
	openFileList[i].inodeId = findFreeInode(filename, CURRENT_WORKING_DIRECTORY); //parent inode unknown
	return(fd);
	}
}

//similar to fsSeek in Linux, based off Professor Bierman's demo in class
uint64_t myfsSeek(int fd, uint64_t position, int method)
{
	//make sure fd is in use
	if(fd >= FDOPENMAX)
		return -1;

	if ((openFileList[fd].flags & FDOPENINUSE) != FDOPENINUSE)
		return -1;

	switch(method)
		{
		case MYSEEK_CUR:
				openFileList[fd].position += position;
				break;

		case MYSEEK_POS:
				openFileList[fd].position = position;
				break;

		case MYSEEK_END:
				openFileList[fd].position = openFileList[fd].size + position;
				break;

		default:
				break;
		}
	return (openFileList[fd].position);
}

//set fd to free
int myfsClose(int fd){
	openFileList[fd].flags = FDOPENFREE;
	openFileList[fd].position = 0;
	free(openFileList[fd].filebuffer);
	openFileList[fd].size = 0;
	return 0;
}
