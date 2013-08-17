#include "package.h"
#include "parser.h"
#include "ast.h"
#include "hash.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#define EXTENSION ".pony"

#define HASH_SIZE 64
#define HASH_MASK (HASH_SIZE - 1)

typedef struct package_t
{
  char* path;
  bool ok;
  ast_t* module;
  struct package_t* next;
} package_t;

typedef struct program_t
{
  package_t* package[HASH_SIZE];
  bool ok;
} program_t;

static bool do_file( package_t* pkg, const char* file )
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
  parser_close( parser );

  ast->sibling = pkg->module;
  pkg->module = ast;

  return true;
}

static bool do_path( package_t* pkg, const char* path )
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
      return do_file( pkg, path );

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

      r &= do_file( pkg, fullpath );
    }
  }

  closedir( dir );
  return r;
}

static package_t* package_get( program_t* program, package_t* from, const char* path )
{
  char composite[FILENAME_MAX];
  char file[FILENAME_MAX];

  if( from != NULL )
  {
    strcpy( composite, from->path );
    strcat( composite, "/" );
  } else {
    composite[0] = '\0';
  }

  strcat( composite, path );

  if( realpath( composite, file ) != file )
  {
    switch( errno )
    {
    case EACCES:
      printf( "Permission denied: %s\n", path );
      break;

    case EINVAL:
    case ENOENT:
    case ENOTDIR:
      printf( "Does not exist: %s\n", path );
      break;

    case EIO:
      printf( "I/O error: %s\n", path );
      break;

    case ELOOP:
      printf( "Symbolic link loop: %s\n", path );
      break;

    case ENAMETOOLONG:
      printf( "Path name too long: %s\n", path );
      break;

    default:
      printf( "Unknown error: %s\n", path );
    }

    return NULL;
  }

  uint64_t hash = strhash( file ) & HASH_MASK;
  package_t* cur = program->package[hash];

  while( cur != NULL )
  {
    if( !strcmp( cur->path, file ) )
    {
      return cur;
    }

    cur = cur->next;
  }

  package_t* pkg = malloc( sizeof(package_t) );
  pkg->path = strdup( file );
  pkg->next = program->package[hash];
  program->package[hash] = pkg;
  pkg->ok = do_path( pkg, file );
  program->ok &= pkg->ok;

  return pkg;
}

static void package_import( program_t* program, package_t* pkg );

static void module_import( program_t* program, package_t* pkg, ast_t* ast )
{
  ast_t* child = ast->child;

  while( child != NULL )
  {
    if( child->t->id == TK_USE )
    {
      package_t* use = package_get( program, pkg, child->child->t->string );
      package_import( program, use );
    }

    child = child->sibling;
  }
}

static void package_import( program_t* program, package_t* pkg )
{
  if( pkg == NULL ) { return; }
  ast_t* ast = pkg->module;

  while( ast != NULL )
  {
    module_import( program, pkg, ast );
    ast = ast->sibling;
  }
}

static void package_free( package_t* pkg )
{
  if( pkg == NULL ) { return; }
  free( pkg->path );
  ast_free( pkg->module );
  package_free( pkg->next );
  free( pkg );
}

static void program_free( program_t* program )
{
  if( program == NULL ) { return; }

  for( int i = 0; i < HASH_SIZE; i++ )
  {
    package_t* cur = program->package[i];
    package_t* next;

    while( cur != NULL )
    {
      next = cur->next;
      package_free( cur );
      cur = next;
    }
  }

  free( program );
}

bool program_compile( const char* path )
{
  program_t* program = calloc( 1, sizeof(program_t) );
  program->ok = true;

  package_t* pkg = package_get( program, NULL, path );
  package_import( program, pkg );

  /* FIX:
   * symbol tables
   * type checking
   * code generation
   */

  bool ret = program->ok;
  program_free( program );
  return ret;
}
