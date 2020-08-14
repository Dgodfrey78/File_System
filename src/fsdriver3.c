/***************************************************************
* Class: CSC-415-03 Spring 2020
* Group Name: Alpha 3
* Name: Dale Armstrong
* StudentID: 920649883
* Name: Danny Godfrey
* StudentID: 
* Name: Be Sun
* StudentID: 920109798
* Name: Brenda Yau
* StudentID: 916959178
* Project: Assignment 3 - File System
* @file: fsdriver3.c
*
* Description: Serves as test driver (main) for the file system.
*   Implements interactive commands: list/create dir,
*   add/remove/copy/mv files, set metadata, copy from normal
    filesys to assignment filesystem and vice versa.
* **************************************************************/

#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FileSystem.h"

#define MAX_INPUT_BUFFER 512
#define MAX_ARG MAX_INPUT_BUFFER/2+1

uint32_t parseArgs(char*, char**);
void processArgs(int, char**);
void run_help(int, char**);
void run_mkdir(int, char**);
void run_ls(int, char**);
void run_lsfs(int, char**);
void run_format(int, char**);
void run_rmdir(int, char**);
void run_cp(int, char**);
void run_mv(int, char**);
void run_del(int, char**);
void run_cpin(int, char**);
void run_cpout(int, char**);
void flushInput();

int main(int argc, char **argv) {
    char* prompt = "fs>";
    char userInput[MAX_INPUT_BUFFER];
    char* args[MAX_ARG];
    uint32_t numArgs;
    char* filename;
    uint64_t volumeSize;
    uint64_t blockSize;

    if (argc < 4) {
		printf("Missing arguments: Filename, Volume Size, Buffer\n");
		exit(EXIT_FAILURE);
    } else if (argc > 4) {
		printf("Too many arguments\n");
		exit(EXIT_FAILURE);
    } else {
		filename = argv[1];
		volumeSize = atoll(argv[2]);
		blockSize = atoll(argv[3]);
    }

    if (volumeSize < blockSize) {
		printf("Error: Volume size must be larger than block size!");
		exit(EXIT_FAILURE);
    }

	int retVal = startPartitionSystem (filename, &volumeSize, &blockSize);
	if (retVal != 0) {
		printf("Error: opening partition %s\n", filename);
		exit(EXIT_FAILURE);
	}

	printf("Opened %s, Volume Size: %llu;  BlockSize: %llu; Return %d\n", filename, (ull_t)volumeSize, (ull_t)blockSize, retVal);

	/* Check if partition is already formatted, if not then format */
	if (!check_fs()) {
		char answer;
		printf("Partition is not formatted.\n");
		printf("You must format the partition to continue.\n");
		printf("Format now? (y/n): ");
		scanf(" %c", &answer);
		flushInput();
		if (answer == 'y' || answer == 'Y') {
			fs_format();
		} else {
			printf("Canceling format.\n");
			printf("Exiting...\n");
			closePartitionSystem();
			exit(EXIT_SUCCESS);
		}
	}

    while (1) {
        printf("%s ", prompt);

        /* retrieve input */
        if (fgets(userInput, MAX_INPUT_BUFFER, stdin) == NULL)
            /* check for EOF */
            if (feof(stdin))
                exit(EXIT_SUCCESS);

        /* test for empty input */
        if (strlen(userInput) == 1) {
            printf("Error: No command entered.\n");
        } else if (strcmp(userInput, "exit\n") == 0) {
        	/* test for exit command */
        	closePartitionSystem();
            exit(EXIT_SUCCESS);
        } else {
            /* check and flush extra data in input stream */
            if (strchr(userInput, '\n') == NULL)
                flushInput();

            numArgs = parseArgs(userInput, args);
            processArgs(numArgs, args);
        }
    }
    return 0;
}

/* Separate arguments into an array */
uint32_t parseArgs(char* input, char** args) {
    int i = 0;
    char* token = strtok(input, " -\t\n");
    while (token != NULL) {
        args[i] = token;
        token = strtok(NULL, " -\t\n");
        i++;
    }
    args[i] = NULL;
    return i;
}

/* run command with given arguments */
void processArgs(int numArgs, char** args) {
	if (strcmp(args[0], "format") == 0) {
		run_format(numArgs, args);
	} else if (strcmp(args[0], "help") == 0) {
		run_help(numArgs, args);
	} else if (strcmp(args[0], "lsfs") == 0) {
		run_lsfs(numArgs, args);
	} else if (strcmp(args[0], "ls") == 0) {
		run_ls(numArgs, args);
	} else if (strcmp(args[0], "mkdir") == 0) {
		run_mkdir(numArgs, args);
	} else if (strcmp(args[0], "rmdir") == 0) {
		run_rmdir(numArgs, args);
	} else if (strcmp(args[0], "cp") == 0) {
		run_cp(numArgs, args);
	} else if (strcmp(args[0], "mv") == 0) {
		run_mv(numArgs, args);
	} else if (strcmp(args[0], "del") == 0) {
		run_del(numArgs, args);
  } else if (strcmp(args[0], "cpin") == 0) {
		run_cpin(numArgs, args);
	} else if (strcmp(args[0], "cpout") == 0) {
		run_cpout(numArgs, args);
	} else {
		printf("%s: command not found\n", args[0]);
		printf("Type help for more info\n");
	}
}

/* Display help information for each function in filesystem */
void run_help(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Too many arguments\n");
		printf("Type help or help <function> for more information\n");
		return;
	} else if (numArgs == 1) {
		printf("Type help <function> to get more information about a function\n");
		printf("Commands:\n");
		printf("format - formats the partition\n");
		printf("lsfs   - lists the information of the current filesystem\n");
		printf("ls     - lists files in the directory\n");
		printf("mkdir  - creates a directory\n");
		printf("rmdir  - removes a directory\n");
		printf("cp     - copies a file from source to destination\n");
		printf("mv     - moves a file from source to destination\n");
		printf("del    - deletes a file\n");

		printf("cpin   - copy a file in from another filesystem\n");
		printf("cpout  - copies a file to another filesystem\n");
		printf("exit   - exit shell\n");
	} else {
		if (strcmp(args[1], "format") == 0) {
			printf("Usage: format\n");
			printf("Formats the partition and installs the filesystem.\n");
			printf("Will delete any current filesystems that are installed\n");
		} else if (strcmp(args[1], "lsfs") == 0) {
			printf("Usage: lsfs\n");
			printf("Lists the information about the current filesystem.\n");
			printf("This includes: Volume name, volume ID, block size, number of blocks,\n");
			printf("	free blocks, and space used.\n");
		} else if (strcmp(args[1], "ls") == 0) {
			printf("Usage: ls\n");
			printf("lists files in the current directory.\n");
		} else if (strcmp(args[1], "mkdir") == 0) {
			printf("Usage: mkdir <dirname>\n");
			printf("Creates a directory with the given name\n");
		} else if (strcmp(args[1], "rmdir") == 0) {
			printf("Usage: rmdir <dirname>\n");
			printf("Deletes the directory with the given name\n");
		} else if (strcmp(args[1], "cp") == 0) {
			printf("Usage: cp <source> <destination>\n");
			printf("Copies the file from the source to the destination\n");
		} else if (strcmp(args[1], "mv") == 0) {
			printf("Usage: mv <source> <destination>\n");
			printf("Moves the file from the source to the destination\n");
		} else if (strcmp(args[1], "del") == 0) {
			printf("Usage: del <filename>\n");
			printf("Deletes the given filename\n");
  	} else if (strcmp(args[1], "cpin") == 0) {
			printf("Usage: cpin <source> <desintation>\n");
			printf("Copies a file from another filesystem into the destination in this filesystem\n");
		} else if (strcmp(args[1], "cpout") == 0) {
			printf("Usage: cpout <source> <destination>\n");
			printf("Copies a file from the current filesystem to another filesystem\n");
		} else {
			printf("Unknown command.\n");
			printf("Type help or help <function> for more information\n");
		}
	}
}

void run_format(int numArgs, char** args) {
	if (numArgs > 1) {
		printf("Unknown arguments\n");
		printf("Usage: format\n");
		return;
	}

	char answer;
	int retvalue;
	printf("Warning: This will delete the current filesystem!\n");
	printf("Do you want to continue? (y/n): ");
	scanf(" %c", &answer);
	flushInput();
	if (answer == 'y' || answer == 'Y') {
		retvalue = fs_format();
	} else {
		printf("Canceled format\n");
		return;
	}

	if (retvalue == -1) {
		printf("Format failed!\n");
	} else if (retvalue == 0) {
		printf("Format success!\n");
	}
}

void run_lsfs(int numArgs, char** args) {
	if (numArgs > 1) {
		printf("Unknown arguments\n");
		printf("Usage: lsfs\n");
		return;
	}

	fs_lsfs();
}

void run_ls(int numArgs, char** args) {
	if (numArgs > 1) {
		printf("Unknown arguments\n");
		printf("Usage: ls\n");
		return;
	}

	fs_ls();
}

void run_mkdir(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Unknown arguments\n");
		printf("Usage: mkdir <dirname>\n");
		return;
	} else if (numArgs < 2) {
		printf("Missing directory name\n");
		printf("Usage: mkdir <dirname>\n");
		return;
	}

	int retvalue = fs_mkdir(args[1]);

	if (retvalue == -1) {
		printf("Could not make directory: %s\n", args[1]);
	} else if (retvalue == -2) {
		printf("%s directory already exists\n", args[1]);
	}
}

void run_rmdir(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Unknown arguments\n");
		printf("Usage: rmdir <dirname>\n");
		return;
	} else if (numArgs < 2) {
		printf("Missing directory name\n");
		printf("Usage: rmdir <dirname>\n");
		return;
	}

	int retvalue = fs_rmdir(args[1]);

	if (retvalue == -1) {
		printf("Could not remove directory: %s\n", args[1]);
	} else if (retvalue == -2) {
		printf("%s directory does not exist\n", args[1]);
	}
}

void run_cp(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: cp <source> <destination>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: cp <source> <destination>\n");
		return;
	}

	int retvalue = fs_cp(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not copy file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

void run_mv(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: mv <source> <destination>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: mv <source> <destination>\n");
		return;
	}

	int retvalue = fs_mv(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not move file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

void run_del(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Unknown arguments\n");
		printf("Usage: del <filename>\n");
		return;
	} else if (numArgs < 2) {
		printf("Missing arguments\n");
		printf("Usage: del <filename>\n");
		return;
	}

	int retvalue = fs_del(args[1]);

	if (retvalue == -1) {
		printf("Could not delete file\n");
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

void run_cpin(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: cpin <source> <desintation>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: cpin <source> <desintation>\n");
		return;
	}

	int retvalue = fs_cpin(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not copy file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

void run_cpout(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: cpout <source> <destination>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: cpout <source> <destination>\n");
		return;
	}

	int retvalue = fs_cpout(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not copy file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

/* flushes the input buffer */
void flushInput() {
    char c;
    while ((c = getchar()) != '\n' && c != EOF)
        ; /* discard characters */
}
