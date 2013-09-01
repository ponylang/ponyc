#include "package.h"
#include "stringtab.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>

int main( int argc, char** argv )
{
  int ret = 0;

  const char* path = ".";
  if( argc > 1 ) { path = argv[1]; }

  ast_t* program = ast_newid( TK_PROGRAM );
  ast_scope( program );

  if( !package_load( program, path ) ) { ret = -1; }

  /* FIX:
   * load builtin types
   * detect imported but unused packages in a module
   *  might be the same code that detects unused vars, fields, etc?
   * code generation
   */

  ast_free( program );
  type_done();
  stringtab_done();

  return ret;
}
