/* library.h
 *
 * part of the Systems Programming assignment
 * (c) Vrije Universiteit Amsterdam, 2005-2015. BSD License applies
 * author  : wdb -_at-_ few.vu.nl
 * contact : arno@cs.vu.nl
 * */

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

typedef int (*server_filterfunc)(char *, int);

typedef int (*server_alter_sample_rate)(int);

typedef int (*server_reverse)(char *, int);

typedef int (*server_verify)(char *);