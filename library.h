#ifndef _LIBRARY_H_
#define _LIBRARY_H_

#include "packet.h"

// this file define encode/decode filter calling conventions
// make sure that your library routines adhere to these functions
//
// ofcourse, when either is useless you do not have to implement it
// for example, there's no reason to create a decoder for a 44.1->22 kHz convertor
#define NONE    "none"
#define INT     "int"

int num_args_allowed;
int int_allowed;
char *arg;
int arg_int;

typedef int (*server_filter)(struct audio_packet *);

typedef int (*server_alter_sample_rate)(int);

typedef int (*server_reverse)(char *, int);

typedef int (*server_verify)(char *);

#endif /* _LIBRARY_H_ */