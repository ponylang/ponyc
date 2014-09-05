#ifndef PONYC_OPTIONS_H
#define PONYC_OPTIONS_H

enum
{
  ARGUMENT_REQUIRED = 0x01,
  ARGUMENT_OPTIONAL = 0x02,
  ARGUMENT_NONE     = 0x04
};

/** Opaque definition of an options parser.
 *
 */
typedef struct options_t options_t;

/** 
 *
 */
typedef struct help_t help_t;

/**
 *
 */
typedef struct arg_t arg_t; //make non-opaque

/** Initializes a command line argument parser for a
 *  fixed amount of arguments to parse.
 */
options_t* options_init(int argc);

/**
 *
 */
void options_add_arg(void* arg_t* arg);

#endif