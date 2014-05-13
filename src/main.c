#include "package.h"
#include "stringtab.h"
#include "types.h"
#include "typechecker.h"
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <getopt.h>
#endif

static struct option opts[] =
{
  {"ast", no_argument, NULL, 'a'},
  {"width", required_argument, NULL, 'w'},
  {NULL, 0, NULL, 0},
};

void usage()
{
  printf(
    "ponyc [OPTIONS] <file>\n"
    "  --ast, -a     print the AST\n"
    "  --width, -w   width to target when printing the AST\n"
    );
}

size_t get_width()
{
  struct winsize ws;
  size_t width = 80;

  if( ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws ) != -1 )
  {
    if( ws.ws_col > width )
      width = ws.ws_col;
  }

  return width;
}

int main( int argc, char** argv )
{
  package_init( argv[0] );

  bool ast = false;
  size_t width = get_width();
  char c;

  while( (c = getopt_long( argc, argv, "aw:", opts, NULL )) != -1 )
  {
    switch( c )
    {
      case 'a': ast = true; break;
      case 'w': width = atoi( optarg ); break;
      default: usage(); return -1;
    }
  }

  argc -= optind;
  argv += optind;

  const char* target = ".";
  if( argc > 0 ) target = argv[0];

  ast_t* program = ast_new( TK_PROGRAM, 0, 0, NULL );
  ast_scope( program );

  int ret = 0;

  if( !typecheck_init( program ) || !package_load( program, target ) )
  {
    ret = -1;
  }

  if( ast )
    ast_print( program, width );

  /* FIX:
   * detect imported but unused packages in a module
   *  might be the same code that detects unused vars, fields, etc?
   * code generation
   */

  ast_free( program );

  print_errors();
  free_errors();

  type_done();
  package_done();
  stringtab_done();

  return ret;
}
