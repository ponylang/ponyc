#include "package.h"
#include <stdlib.h>
#include <string.h>

int main( int argc, char** argv )
{
  const char* path = ".";
  if( argc > 1 ) { path = argv[1]; }

  ast_t* program = ast_newid( TK_PROGRAM );

  if( !package_load( program, path ) )
  {
    ast_free( program );
    return -1;
  }

  /* FIX:
   * type checking
   * detect unused packages
   *  might be the same code that detects unused vars, fields, etc?
   * code generation
   */

  ast_free( program );
  return 0;
}
