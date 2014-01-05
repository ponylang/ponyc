#include "package.h"
#include "stringtab.h"
#include "types.h"
#include "typechecker.h"

int main( int argc, char** argv )
{
  package_init( argv[0] );

  const char* target = ".";
  if( argc > 1 ) target = argv[1];

  ast_t* program = ast_newid( TK_PROGRAM );
  ast_scope( program );

  int ret = 0;

  if( !typecheck_init( program ) || !package_load( program, target ) )
  {
    ret = -1;
  }

  /* FIX:
   * detect imported but unused packages in a module
   *  might be the same code that detects unused vars, fields, etc?
   * code generation
   */

  ast_free( program );
  type_done();
  package_done();
  stringtab_done();

  return ret;
}
