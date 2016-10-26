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

char *args_allowed[] = {"left", "right", "mid", INT};
num_args_allowed = 4;
int_allowed = 0;
int arg_min = 0;
int arg_max = 200;

void _init()
{
    printf("Initializing library\n");

    // Set the int_allowed flag if relevant
    for(int i = 0; i < num_args_allowed; i++) {
        if(strcmp(INT, args_allowed[i]) == 0) {
            int_allowed = 1;
        }
    }
}

int filter(char *buffer, int bufferlen) {
    printf("lib filter here\n");
    return 0;
}

int alter_sample_rate(int sample_rate) {
    printf("Altering sample rate\n");
    float percentage = (float) arg_int / 100;
    float result_float = (float) sample_rate * percentage;
    int result = (int) result_float;
    printf("%d * %f = %f (%d)\n", sample_rate, percentage, result_float, result);
    return result;
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
    while(i < num_args_allowed) {
        // Does one of the strings match ARG?
        if(strcmp(libarg, args_allowed[i]) == 0) {
            printf("strlen: %d\n", strlen(libarg));
            // TODO copy string
            arg = "Hello";
            return 0;
        }

        i ++;
    }

    // Return a special value if no argument was provided, but one is required
    if(strcmp(libarg, NONE) == 0) {
        return -2;
    }

    // Are integers allowed and is an integer provided as argument?
    if(int_allowed) {
        for(int i = 0; i < strlen(libarg); i ++) {
            if(isdigit(libarg[i]) == 0) {
                return -1;
            }
        }

        arg_int = atoi(libarg);

        // TODO debug
        printf("Arg int: %d\n", arg_int);

        if(arg_int < arg_min || arg_int > arg_max) {
            printf("Library argument not withint boundaries of %d and %d\n", arg_min, arg_max);
            return -1;
        }

        return 0;
    }


    return -1;
}


/* library's cleanup function */
void _fini()
{
	printf("Cleaning out library\n");
}

