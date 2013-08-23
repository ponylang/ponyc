#include "package.h"
#include "parser.h"
#include "ast.h"
#include "hash.h"
#include "stringtab.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

#define EXTENSION ".pony"

static bool do_file( ast_t* parent, const char* file )
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
    parser_printerrors( parser );
    parser_close( parser );
    return false;
  }

  ast_t* ast = parser_ast( parser );
  ast_add( parent, ast );
  parser_close( parser );

  return true;
}

static bool do_path( ast_t* parent, const char* path )
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
      return do_file( parent, path );

    default:
      printf( "Unknown error: %s\n", path );
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

      r &= do_file( parent, fullpath );
    }
  }

  closedir( dir );
  return r;
}

static bool use_prep( ast_t* use )
{
  ast_t* module = ast_parent( use );
  ast_t* path = ast_child( use );
  ast_t* id = ast_sibling( path );

  if( !package_load( module, ast_name( path ) ) ) { return false; }

  ast_t* package = ast_get( module, ast_name( path ) );

  if( !ast_set( module, ast_name( id ), package ) )
  {
    printf( "Can't reuse import ID: %s\n", ast_name( id ) );
    return false;
  }

  return true;
}

static bool module_prep( ast_t* module )
{
  bool ret = true;
  ast_t* child = ast_child( module );

  while( child != NULL )
  {
    switch( ast_id( child ) )
    {
      case TK_USE:
        ret &= use_prep( child );
        break;

      case TK_ALIAS:
        break;

      case TK_CLASS:
        break;

      default: assert( 0 );
    }

    child = ast_sibling( child );
  }

  return ret;
}

bool package_load( ast_t* from, const char* path )
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];
  ast_t* program;

  if( ast_id( from ) == TK_PROGRAM )
  {
    program = from;
    composite[0] = '\0';
  } else {
    while( ast_id( from ) != TK_PACKAGE )
    {
      from = ast_parent( from );
    }

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
      printf( "Permission denied: %s\n", composite );
      break;

    case EINVAL:
    case ENOENT:
    case ENOTDIR:
      printf( "Does not exist: %s\n", composite );
      break;

    case EIO:
      printf( "I/O error: %s\n", composite );
      break;

    case ELOOP:
      printf( "Symbolic link loop: %s\n", composite );
      break;

    case ENAMETOOLONG:
      printf( "Path name too long: %s\n", composite );
      break;

    default:
      printf( "Unknown error: %s\n", composite );
    }

    return false;
  }

  const char* name = stringtab( file );

  if( ast_get( program, name ) != NULL ) { return true; }

  ast_t* package = ast_new( TK_PACKAGE, 0, 0, (void*)name );
  ast_add( program, package );
  ast_set( program, name, package );

  if( !do_path( package, name ) ) { return false; }

  bool ret = true;

  ast_t* module = ast_child( package );

  while( module != NULL )
  {
    ret &= module_prep( module );
    module = ast_sibling( module );
  }

  return ret;
}

bool package_start( const char* path )
{
  ast_t* program = ast_newid( TK_PROGRAM );
  if( !package_load( program, path ) ) { return false; }

  /* FIX:
   * add types to package symbol table
   * type checking
   * code generation
   */

  ast_free( program );
  return true;
}
