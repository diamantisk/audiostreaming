/* libblank.c
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "library.h"
#include "packet.h"

char *ARGS[] = {"left", "right", "mid", NONE};
int NUMARGS = 4;

/* library's initialization function */
void _init()
{
	printf("Initializing library\n");
}

int encode(char* buffer, int bufferlen)
{
    printf("encoding\n");
	return bufferlen;
}

char *decode(char* buffer, int bufferlen, int *outbufferlen)
{
    printf("decoding\n");
	*outbufferlen = bufferlen;
	return buffer;
}

/** Verify whether the filter argument is applicable to the library
 *  TODO expand
 */
int verify_arg(char *libarg) {
    int i = 0;
    while(i < NUMARGS) {
        if(strcmp(libarg, ARGS[i]) == 0) {
            return 0;
        }
        i ++;
    }

    return -1;
}


/* library's cleanup function */
void _fini()
{
	printf("Cleaning out library\n");
}

