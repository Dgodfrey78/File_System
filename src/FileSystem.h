//gcc fsdriver3.c FileSystem.c fsLow.c -lm -o test


#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "fsLow.h"

#define SUPER_SIGNATURE 0x44616c6541726d73
#define SUPER_SIGNATURE2 0x736d7241656c6144
#define NUM_DIRECT 10
#define NUM_INDIRECT 2
#define BLOCKS_PER_INODE 4

#define DIRECTORY_TYPE 1
#define FILE_TYPE 2
#define USED_FLAG 0xFF
#define UNUSED_FLAG 0

#define MAX_PATH_NAME 4096
#define MAX_DIRECTORIES 1024
#define MAX_NAME_SIZE 128
#define FDOPENMAX 50
#define FDOPENFREE 0x00000002
#define FDOPENINUSE 0x00000001
#define FDOPENFORREAD 0x00000020  //0000 0000 0000 0000 0000 0000 0001 0000
#define FDOPENFORWRITE 0x00000010 ////0000 0000 0000 0000 0000 0000 0010 0000

#define MYSEEK_CUR 1
#define MYSEEK_POS 2
#define MYSEEK_END 3
#define CURRENT_WORKING_DIRECTORY 5  //temporary number for myfsOpen function

/* Volume Control Block */
typedef struct SuperBlock {
    uint64_t superSignature;		//Signature for file system
	  uint64_t inodeStart;			//Pointer to inode starting block
    uint64_t numInodes;				//Total number of inodes
    uint64_t usedInodes;			//Number of used inodes
    uint64_t bitVectorStart;		//Pointer to free blocks bitmap
    uint64_t freeBlocks;			//The number of free blocks not in use
    uint64_t usedBlocks;			//Number of used blocks
    uint64_t totalDataBlocks;		//Total Number of Data Blocks
    uint64_t maxPointersPerIndirect[NUM_INDIRECT]; //Max pointers per indirect block
    uint64_t rootDataPointer;		//Pointer to root data block, also start of data blocks
    uint64_t superSignature2;
} SuperBlock, *SuperBlock_p;

/* Inodes to point to data */
typedef struct Inode {
	char used;							//Whether this Inode is in use
	uint32_t type;						//File or Directory
	uint64_t parent_p;					//Pointer to parent inode
	uint64_t size;                      //Size of file in bytes
    uint64_t inode;
    uint64_t blocksReserved;
	uint64_t dateModified;				//Date when the file/directory was last modified
	uint64_t directData[NUM_DIRECT]; 	//Pointers directly to data blocks
	uint64_t indirectData[NUM_INDIRECT];//Pointers to data block that points to other data blocks
} Inode, *Inode_p;

/* File Control Block(FCB) */
typedef struct FCB
{
	uint64_t inodeID;				//Number of inode
	char name[MAX_NAME_SIZE];		//Name of file
} FCB, *FCB_p;

typedef struct openFileEntry
{
  int flags;
  uint64_t position; //where in the file am i
  uint64_t inodeId;   //number of inode of file
  char * filebuffer;
}openFileEntry, openFileEntry_p;

openFileEntry * openFileList; //array of currently open files

/* Current working path */
typedef struct WorkingDirectory {
    FCB directories[MAX_DIRECTORIES];
    uint32_t numberOfDirectories;
    char pathName[MAX_PATH_NAME];
    Inode_p directoryInode;
    FCB_p directoryData;
} WorkingDirectory, *WorkingDirectory_p;

extern SuperBlock_p sb;

/**
 * Checks if a file system exists on the current partition.
 * returns 1 if filesystem exists
 * returns 0 if filesystem does not exist
 */
int check_fs();

/**
 * Formats the current partition and installs a new filesystem.
 * returns 0 if format was successful
 * returns -1 if format was unsuccessful
 */
int fs_format();

/** Outputs data about the current filesystem */
void fs_lsfs();

/** Lists the files in the current directory */
void fs_ls();

/**
 * Creates a directory of the given name.
 * returns 0 if sucessful
 * returns -1 if it could not create directory
 * returns -2 if directory already exists
 */
int fs_mkdir(char* directoryName);

/**
 * Deletes the directory with the given name.
 * returns 0 if successful
 * returns -1 if it could not delete directory
 * returns -2 if directory does not exist
 */
int fs_rmdir(char* directoryName);

/**
 * Copies the file from source to destination
 * returns 0 if successful
 * returns -1 if could not copy file
 * returns -2 if source file does not exist
 */
int fs_cp(char* sourceFile, char* destFile);

/**
 * Moves the file from source to destination
 * returns 0 if successful
 * returns -1 if could not move file
 * returns -2 if source file does not exist
 */
int fs_mv(char* sourceFile, char* destFile);

/**
 * Deletes the file with the given name
 * returns 0 if successful
 * returns -1 if could not delete file
 * returns -2 if file does not exist
 */
int fs_del(char* filename);

/**
 * Copies a file from another filesystem to destination
 * in current filesystem.
 * returns 0 if successful
 * returns -1 if unsuccessful
 * returns -2 if source file does not exist
 */
int fs_cpin(char* sourceFile, char* destFile);

/**
 * Copies a file from this filesystem to another filesystem
 * returns 0 if successful
 * returns -1 if unsuccessful
 * returns -2 if source file does not exist
 */
int fs_cpout(char* sourceFile, char* destFile);

/**
 * Given an inode number and an Inode_p pointer, readInode
 * will read from the request inode into either a buffer already
 * preallocated, or will allocate a buffer if the pointer is null.
 * Returns 0 if successful
 * Returns -1 if unsuccessful
 */
int readInode(uint64_t inodeID, Inode_p inodeBuffer);

/**
 * Given an inode number and an Inode_p buffer, writeInode
 * will write to the given inode number from the provided buffer.
 * The inode buffer must be preallocated with the contents.
 * Returns 0 if successful
 * Returns -1 if unsuccessful
 */
int writeInode(uint64_t inodeID, Inode_p inodeBuffer);

/**
 * Reads the data of the file from the filesystem and stores it into the destination.
 * If the pointer is null, then memory will be allocated to hold the file data.
 * @param destination the buffer that the file data will be stored in.
 * @param inodeID the file's inode.
 * @param length is the number of bytes to read. 0 if reading the entire file.
 * Returns the number of bytes read into destination if successful
 * Returns -1 if unsuccessful
 */
uint64_t readFile(char* destination, const uint64_t inodeID, const uint64_t length);

/**
 * Finds and returns a free inode number in the inode table.
 * Returns 0 if unsuccessful
 * Returns a free inode
 */
uint64_t findFreeInode(char* name, uint64_t parentInode);

/**
 * Hashes the name of the file/folder along with the parent
 * inode and should return an unused inode
 * Returns the hash
 */
uint64_t hashInode(char* name, uint64_t parentInode);

/**
 * Finds the next closest prime number to the given minimum
 * block size. This allows for the hash function to be more
 * efficient.
 * Returns the next closest Prime number to the given minBlockSize
 */
uint64_t findNextPrime(uint64_t minBlockSize);

int myfsClose(int fd);

int myfsOpen(char * filename);

#endif
