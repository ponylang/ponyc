#include "package.h"
#include "stringtab.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>

int main( int argc, char** argv )
{
  type_init();

  const char* path = ".";
  if( argc > 1 ) { path = argv[1]; }

  ast_t* program = ast_newid( TK_PROGRAM );

  if( !package_load( program, path ) )
  {
    ast_free( program );
    return -1;
  }

  /* FIX:
   * detect unused packages
   *  might be the same code that detects unused vars, fields, etc?
   * code generation
   */

  ast_free( program );

  type_done();
  stringtab_done();
  return 0;
}
