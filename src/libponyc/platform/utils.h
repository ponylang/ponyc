#ifndef PLATFORM_UTILS_H
#define PLATFORM_UTILS_H

#ifdef PLATFORM_IS_VISUAL_STUDIO
#  include <malloc.h>
#  define VLA(TYPE, NAME, SIZE) TYPE* NAME = (TYPE*) alloca(\
            (SIZE)*sizeof(TYPE))
#endif

#ifdef PLATFORM_IS_WINDOWS
#  define no_argument 0x00
#  define required_argument 0x01
#  define optional_argument 0x02

extern int opterr, optind, optopt, optreset;
extern char* optarg;

struct option
{
  const char* name;
  int has_arg;
  int *flag;
  int val;
};

int getopt(int argc, char* const argv[], const char* optstring);

int getopt_long(int argc, char* const argv[], const char* optstring,
  const struct option* longopts, int* longindex);

struct winsize
{
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
};
#else
#  include <unistd.h>
#  include <getopt.h>
#  include <sys/ioctl.h>
#endif

#if defined(PLATFORM_IS_POSIX_BASED) || defined(PLATFORM_IS_CLANG_OR_GCC)
#  define VLA(TYPE, NAME, SIZE) TYPE NAME[(SIZE)]
#endif

void pony_strcpy(char* dest, const char* source);

void pony_strncpy(char* dest, const char* source, size_t len);

void pony_strcat(char* dest, const char* appendix);

char* pony_strdup(const char* source);

bool pony_get_term_winsize(struct winsize* ws);

#endif
