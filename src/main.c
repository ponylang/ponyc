#include "parser.h"
#include "typechecker.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#define EXTENSION ".pony"

void print_errors( errorlist_t* el, const char* file )
{
  error_t* e = el->head;

  while( e != NULL )
  {
    printf( "%s [%ld:%ld]: %s\n", file, e->line, e->pos, e->msg );
    e = e->next;
  }
}

bool do_file( const char* file )
{
  parser_t* parser = parser_open( file );

  if( parser == NULL )
  {
    printf( "Couldn't open: %s\n", file );
    return false;
  }

  errorlist_t* el = parser_errors( parser );

  if( el->count > 0 )
  {
    print_errors( el, file );
    errorlist_free( el );
    parser_close( parser );
    return false;
  }

  ast_t* ast = parser_ast( parser );
  ast_free( ast );
  errorlist_free( el );
  parser_close( parser );
  return true;
}

bool do_path( const char* path )
{
  DIR* dir = opendir( path );

  if( dir == NULL )
  {
    switch( errno )
    {
    case EACCES:
      printf( "Permission denied: %s\n", path );
      break;

    case ENOENT:
      printf( "Does not exist: %s\n", path );
      break;

    case ENOTDIR:
      return do_file( path );

    default:
      printf( "Unknown error: %s\n", path );
    }

    return false;
  }

  struct dirent dirent;
  struct dirent* d;
  bool r = true;

  // this isn't thread-safe
  while( !readdir_r( dir, &dirent, &d ) && (d != NULL) )
  {
    if( d->d_type & DT_DIR )
    {
      // skip ".", ".." and hidden directories
      if( d->d_name[0] == '.' ) { continue; }

      char fullpath[FILENAME_MAX];
      strcpy( fullpath, path );
      strcat( fullpath, "/" );
      strcat( fullpath, d->d_name );

      r &= do_path( fullpath );
    } else if( d->d_type & DT_REG ) {
      // handle only files with the specified extension
      const char* p = strrchr( d->d_name, '.' );
      if( !p || strcmp( p, EXTENSION ) ) { continue; }

      char fullpath[FILENAME_MAX];
      strcpy( fullpath, path );
      strcat( fullpath, "/" );
      strcat( fullpath, d->d_name );

      r &= do_file( fullpath );
    }
  }

  closedir( dir );
  return r;
}

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

  if( !do_path( path ) ) { return -1; }
  return 0;
}
