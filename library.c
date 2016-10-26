#include <string.h>
#include "library.h"

/** Verify whether the filter argument is applicable to the library
 *  TODO expand
 */
int verify_arg(char *libarg) {
    int i = 0;
    while(i < num_args_accepted) {
        // Does one of the strings match ARG?
        if(strcmp(libarg, args_accepted[i]) == 0) {
            return 0;
        }
        i ++;
    }

    return -1;
}