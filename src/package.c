#include "package.h"
#include "parser.h"
#include "ast.h"
#include "stringtab.h"
#include "typechecker.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#define EXTENSION ".pony"

typedef struct path_t
{
  const char* path;
  struct path_t* next;
} path_t;

static path_t* search;

static bool do_file( ast_t* package, const char* file )
{
  source_t* source = source_open( file );
  if( source == NULL ) { return false; }

  ast_t* module = parse( source );

  if( module == NULL )
  {
    source_close( source );
    return false;
  }

  ast_add( package, module );
  return true;
}

static bool do_path( ast_t* package, const char* path )
{
  DIR* dir = opendir( path );

  if( dir == NULL )
  {
    switch( errno )
    {
      case EACCES:
        errorf( path, "permission denied" );
        break;

      case ENOENT:
        errorf( path, "does not exist" );
        break;

      case ENOTDIR:
        return do_file( package, path );

      default:
        errorf( path, "unknown error" );
    }

    return false;
  }

  struct dirent dirent;
  struct dirent* d;
  bool r = true;

  while( !readdir_r( dir, &dirent, &d ) && (d != NULL) )
  {
    if( d->d_type & DT_REG )
    {
      // handle only files with the specified extension
      const char* p = strrchr( d->d_name, '.' );
      if( !p || strcmp( p, EXTENSION ) ) { continue; }

      char fullpath[FILENAME_MAX];
      strcpy( fullpath, path );
      strcat( fullpath, "/" );
      strcat( fullpath, d->d_name );

      r &= do_file( package, fullpath );
    }
  }

  closedir( dir );
  return r;
}

const char* try_path( const char* base, const char* path )
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];

  if( base != NULL )
  {
    strcpy( composite, base );
    strcat( composite, "/" );
    strcat( composite, path );
  } else {
    strcpy( composite, path );
  }

  if( realpath( composite, file ) != file ) { return NULL; }

  return stringtab( file );
}

const char* find_path( ast_t* from, const char* path )
{
  // absolute path
  if( path[0] == '/' ) { return try_path( NULL, path ); }

  const char* result;

  if( ast_id( from ) == TK_PROGRAM )
  {
    // try a path relative to the current working directory
    result = try_path( NULL, path );
    if( result != NULL ) { return result; }
  } else {
    // try a path relative to the importing package
    from = ast_nearest( from, TK_PACKAGE );
    result = try_path( ast_data( from ), path );
    if( result != NULL ) { return result; }
  }

  // try the search paths
  path_t* p = search;

  while( p != NULL )
  {
    result = try_path( p->path, path );
    if( result != NULL ) { return result; }
    p = p->next;
  }

  return NULL;
}

void package_addpath( const char* path )
{
  path_t* p = malloc( sizeof(path_t) );
  p->path = stringtab( path );
  p->next = search;
  search = p;
}

ast_t* package_load( ast_t* from, const char* path )
{
  const char* name = find_path( from, path );
  if( name == NULL ) { return NULL; }

  ast_t* program = ast_nearest( from, TK_PROGRAM );
  ast_t* package = ast_get( program, name );

  if( package != NULL ) { return package; }

  package = ast_new( TK_PACKAGE, 0, 0, (void*)name );
  ast_scope( package );
  ast_add( program, package );
  ast_set( program, name, package );

  printf( "=== Building %s ===\n", name );
  if( !do_path( package, name ) ) { return NULL; }
  if( !typecheck( package ) ) { return NULL; }

  return package;
}

void package_done()
{
  path_t* next;

  while( search != NULL )
  {
    next = search->next;
    free( search );
    search = next;
  }
}
