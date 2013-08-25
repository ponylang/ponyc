#include "package.h"
#include <stdlib.h>
#include <string.h>

int main( int argc, char** argv )
{
  const char* path = ".";

  if( argc > 1 )
  {
    // trim trailing slashes
    size_t len = strlen( argv[1] );

    while( (len > 0) && (argv[1][len - 1] == '/') )
    {
      argv[1][--len] = '\0';
    }

    path = argv[1];
  }

  ast_t* program = package_start( path );

  if( program == NULL ) { return -1; }

  ast_free( program );
  return 0;
}
