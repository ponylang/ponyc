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

  if( !package_start( path ) ) { return -1; }
  return 0;
}
