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

static bool import_package( ast_t* use )
{
  ast_t* module = ast_parent( use );
  ast_t* path = ast_child( use );
  ast_t* id = ast_sibling( path );
  const char* name = ast_name( id );

  if( (name[0] >= 'A') && (name[0] <= 'Z') )
  {
    ast_error( id, "package name '%s' can't start A-Z", name );
    return false;
  }

  ast_t* package = package_load( module, ast_name( path ) );

  if( package == NULL )
  {
    ast_error( use, "can't load package '%s'", ast_name( path ) );
    return false;
  }

  if( !ast_set( module, name, package ) )
  {
    ast_error( id, "can't reuse import ID '%s'", name );
    return false;
  }

  return true;
}

static bool import_packages( ast_t* module )
{
  bool ret = true;
  ast_t* child = ast_child( module );

  while( child != NULL )
  {
    if( ast_id( child ) == TK_USE )
    {
      ret &= import_package( child );
    }

    child = ast_sibling( child );
  }

  return ret;
}

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

  return import_packages( module );
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

ast_t* package_load( ast_t* from, const char* path )
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];
  ast_t* program;

  if( (path[0] == '/') || (ast_id( from ) == TK_PROGRAM) )
  {
    program = from;
    composite[0] = '\0';
  } else {
    from = ast_nearest( from, TK_PACKAGE );
    program = ast_parent( from );
    strcpy( composite, ast_data( from ) );
    strcat( composite, "/" );
  }

  strcat( composite, path );

  if( realpath( composite, file ) != file )
  {
    switch( errno )
    {
      case EACCES:
        errorf( composite, "permission denied" );
        break;

      case EINVAL:
      case ENOENT:
      case ENOTDIR:
        errorf( composite, "does not exist" );
        break;

      case EIO:
        errorf( composite, "I/O error" );
        break;

      case ELOOP:
        errorf( composite, "symbolic link loop" );
        break;

      case ENAMETOOLONG:
        errorf( composite, "path name too long" );
        break;

      default:
        errorf( composite, "unknown error" );
    }

    return NULL;
  }

  const char* name = stringtab( file );
  ast_t* package = ast_get( program, name );

  if( package != NULL ) { return package; }

  package = ast_new( TK_PACKAGE, 0, 0, (void*)name );
  ast_add( program, package );
  ast_set( program, name, package );

  printf( "=== Building %s ===\n", name );
  if( !do_path( package, name ) ) { return NULL; }
  if( !typecheck( package ) ) { return NULL; }

  return package;
}
