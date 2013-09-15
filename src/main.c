#include "package.h"
#include "stringtab.h"
#include "types.h"
#include "typechecker.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool filepath( const char *file, char* path )
{
  struct stat sb;

  if( (realpath( file, path ) != path)
    || (stat( path, &sb ) != 0)
    || ((sb.st_mode & S_IFMT) != S_IFREG)
    )
  {
    return false;
  }

  char* p = strrchr( path, '/' );

  if( p != NULL )
  {
    if( p != path )
    {
      p[0] = '\0';
    } else {
      p[1] = '\0';
    }
  }

  return true;
}

static bool execpath( const char* file, char* path )
{
  // if it contains a separator of any kind, it's an absolute or relative path
  if( strchr( file, '/' ) != NULL ) { return filepath( file, path ); }

  // it's just an executable name, so walk the path
  const char* env = getenv( "PATH" );

  if( env != NULL )
  {
    size_t flen = strlen( file );

    while( true )
    {
      char* p = strchr( env, ':' );
      size_t len;

      if( p != NULL )
      {
        len = p - env;
      } else {
        len = strlen( env );
      }

      if( (len + flen + 1) < FILENAME_MAX )
      {
        char check[FILENAME_MAX];
        strncpy( check, env, len );
        check[len++] = '/';
        strcpy( &check[len], file );

        if( filepath( check, path ) ) { return true; }
      }

      if( p == NULL ) { break; }
      env = p + 1;
    }
  }

  // try the current directory as a last resort
  return filepath( file, path );
}

static void ponypath()
{
  const char* env = getenv( "PONYPATH" );
  if( env == NULL ) { return; }

  while( true )
  {
    char* p = strchr( env, ':' );
    size_t len;

    if( p != NULL )
    {
      len = p - env;
    } else {
      len = strlen( env );
    }

    if( len < FILENAME_MAX )
    {
      char path[FILENAME_MAX];
      strncpy( path, env, len );
      path[len] = '\0';
      package_addpath( path );
    }

    if( p == NULL ) { break; }
    env = p + 1;
  }
}

int main( int argc, char** argv )
{
  ponypath();

  char path[FILENAME_MAX];

  if( execpath( argv[0], path ) )
  {
    strcat( path, "/packages" );
    package_addpath( path );
  }

  const char* target = ".";
  if( argc > 1 ) { target = argv[1]; }

  ast_t* program = ast_newid( TK_PROGRAM );
  ast_scope( program );

  int ret = 0;

  if( !typecheck_init( program ) ||
      !package_load( program, target )
    )
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
