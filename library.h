#ifndef _LIBRARY_H_
#define _LIBRARY_H_

#include "packet.h"

// Convenient global defines
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