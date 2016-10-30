#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "library.h"
#include "packet.h"

char *args_allowed[] = {"half", "double", "triple", "min", "max", INT};
int num_args_allowed = 6;
int int_allowed = 0;
int arg_min = 10;
int arg_max = 400;
int percentage;

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

int alter_sample_rate(int sample_rate) {
    if(arg) {
        if(strcmp("half", arg) == 0) {
            arg_int = 50;
        } else
        if(strcmp("double", arg) == 0) {
            arg_int = 200;
        } else
        if(strcmp("triple", arg) == 0) {
            arg_int = 300;
        } else
        if(strcmp("min", arg) == 0) {
            arg_int = arg_min;
        } else
        if(strcmp("max", arg) == 0) {
            arg_int = arg_max;
        }
    }

    float percentage = (float) arg_int / 100;
    float result_float = (float) sample_rate * percentage;
    return (int) result_float;
}

int verify_arg(char *libarg) {
    int i = 0;
    while(i < num_args_allowed) {
        // Store the string in the library if it is a valid argument
        if(strcmp(libarg, args_allowed[i]) == 0) {
            arg = malloc(sizeof(char) * (strlen(libarg) + 1));
            strncpy(arg, libarg, strlen(libarg));
            arg[strlen(libarg)] = '\0';
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

        if(arg_int < arg_min || arg_int > arg_max) {
            printf("Library argument not within boundaries of %d and %d\n", arg_min, arg_max);
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
    arg_int = 0;
    if(arg != NULL)
        free(arg);
}

