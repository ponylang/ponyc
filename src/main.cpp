#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#define EXTENSION ".pony"

bool do_file( const char* file )
{
  printf( "File %s\n", file );
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
      printf( "Not a directory: %s\n", path );
      break;

    default:
      printf( "Unknown error: %s\n", path );
    }

    return false;
  }

  struct dirent* d;
  bool r = true;

  while( (d = readdir( dir )) != NULL )
  {
    if( d->d_type & DT_DIR )
    {
      if( d->d_name[0] == '.' ) { continue; }

      char fullpath[FILENAME_MAX];
      strcpy( fullpath, path );
      strcat( fullpath, "/" );
      strcat( fullpath, d->d_name );

      r &= do_path( fullpath );
    } else if( d->d_type & DT_REG ) {
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
  if( argc > 1 ) { path = argv[1]; }
  if( !do_path( path ) ) { return -1; }
  return 0;
}
