/**
 * This file is an altered version of the reverse.c made in assignment 2.
 * It boils down to this file printing its output to
 *  a separate file and adopting library behaviour.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "library.h"

char *args_allowed[] = {NONE};
int num_args_allowed = 1;

int bufsize = 1024;
int wavheader_size = 44;
int filesize_max = 1000000; // bytes
char *filename_write;
char *filename_new = ".reverse";

struct node {
    void *buf;
    int name;
    int size;
    struct node *next;
};

void _init()
{
    printf("Initializing library\n");

    // Remove the existing reverse file if it exists (only applicable to debugging or manual aborts)
    remove(filename_write);
}

int verify_arg(char *libarg) {
    if (strcmp(libarg, NONE) == 0)
        return 0;

    return -1;
}

/**
 * Reverse the contents of a buffer
 * @param str The buffer to use, casted to a string
 * @param n   The length of the buffer
 * @param channels the amount of channels of the audio file
 */
void reverseBuffer(char *str, int n, int channels) {
    int i;

    for (i = 0; i < n / 2; ++i) {
        char tmp = str[i];
        char end = str[n - 1 - i];
        str[i] = end;
        str[n - 1 - i] = tmp;
    }

    if(channels == 2) {
        for (i = 0; i < n; i += 2) {
            char tmp = str[i];
            str[i] = str[i + 1];
            str[i + 1] = tmp;
            str[i] = 0;
//            str[i + 1] = 0;
        }
    }

//    stereo stuff
//    for(i = 0; i < n; i += 4) {
//        str[i + 2] = 0;
//        str[i + 3] = 0;
//    }
}

/**
 * Recursively write the contents of linked list members to stdout
 * @param  current       The current member of the linked list
 * @param  numberOfNodes The total number of nodes in the list
 * @return               0 on success
 */
int writeRecursive(struct node *current, int numberOfNodes, int wd, int channels) {
    int err;
    reverseBuffer(current->buf, current->size, channels);

//    printf("WRITING STUFF TO WD: %d\n", wd);

    if(current->name == numberOfNodes) {
        err = write(wd, current->buf, current->size);
    } else {
        writeRecursive(current->next, numberOfNodes, wd, channels);
        err = write(wd, current->buf, current->size);
    }

    if(err < 0) {
        perror("Error while writing file");
    }

    return 0;
}

int reverse(char *filename, int channels) {
    int numberOfNodes = 0;

    // Verify the file size (large files lead to a heap memory exhaustion because of the recursive malloc)
    FILE *fp;
    fp = fopen(filename, "r");
    if(fp < 0) {
        printf("Unable to open file %s\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    fclose(fp);

    if(filesize > filesize_max) {
        printf("Filesize too large\n");
        return -1;
    }

    int fd = open(filename, O_RDONLY);
    if(fd < 0) {
        printf("Unable to open file %s\n", filename);
        return -1;
    }

//    int filename_write_size = strlen(filename_new);
    filename_write = (char *) malloc(sizeof(char) * strlen(filename_new));
    strncpy(filename_write, filename_new, strlen(filename_new));
    filename_write[strlen(filename_new)] = '\0';

    // TODO debug
//    printf("filename: %s (%lu)\n", filename, strlen(filename));
//
//    printf("filename_write: %s (%lu, expected %d)\n", filename_write, strlen(filename_write), filename_write_size);

    int wd = open(filename_write, O_CREAT | O_RDWR, S_IRWXU);
    if(wd == -1) {
        printf("Unable to create file %s\n", filename_write);
        return 1;
    }

    // Initialize root node of linked list
    struct node *root = (struct node *) malloc(sizeof(struct node));
    root->next = 0;
    root->name = 0;
    root->buf = malloc(bufsize);


    // Preserve the wav header
    char *wavheader_buf = (char *) malloc(sizeof(char) * wavheader_size);

    int wavheader = read(fd, wavheader_buf, wavheader_size);
    if(wavheader < 0) {
        perror("Error while reading file");
    }

    write(wd, wavheader_buf, wavheader_size);

    int bytes = read(fd, root->buf, bufsize);
    if(bytes < 0) {
        perror("Error while reading file");
    }

    root->size = bytes;

    struct node *previous = root;

    // Loop over the file and allocate list members
    while(bytes == bufsize) {
        struct node *current = malloc(sizeof(struct node));
        current->buf = malloc(bufsize);

        bytes = read(fd, current->buf, bufsize);
        if(bytes < 0) {
            perror("Error while reading file");
        } else if(bytes > 0) {
            numberOfNodes ++;
            current->name = numberOfNodes;
            current->size = bytes;

            previous->next = current;
            previous = current;
        } else {
            // Free allocated node if the final read produced 0 bytes
            free(current->buf);
            free(current);
        }
    }

    // Start writing to stdout
    writeRecursive(root, numberOfNodes, wd, channels);

    struct node *current = root;
    struct node *next;

    // Free the linked list
    while(current->name < numberOfNodes) {
        next = current->next;
        free(current->buf);
        free(current);
        current = next;
    }

    // Free the final element of The linked list
    free(current->buf);
    free(current);

    // printf("content of previous: %s\n", previous->buf);

    close(fd);
    close(wd);

    strncpy(filename, filename_new, strlen(filename_new));
    filename[strlen(filename_new)] = '\0';

    return 0;
}

/* library's cleanup function */
void _fini()
{
    int err;
    printf("Cleaning out library\n");

    err = remove(filename_write);
    if(err < 0) {
        perror("Failed to delete .reverse file");
    }

    printf("Removed %s\n", filename_write);
    free(filename_write);
}