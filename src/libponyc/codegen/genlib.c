#include "genlib.h"
#include "genobj.h"
#include "gentype.h"
#include "genprim.h"
#include <string.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#else
   //disable warnings of unlink being deprecated
#  pragma warning(disable:4996)
#endif

static bool generate_actors(compile_t* c, ast_t* program)
{
  // Look for C-API actors in every package.
  bool found = false;
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    ast_t* module = ast_child(package);

    while(module != NULL)
    {
      ast_t* entity = ast_child(module);

      while(entity != NULL)
      {
        if(ast_id(entity) == TK_ACTOR)
        {
          ast_t* c_api = ast_childidx(entity, 5);

          if(ast_id(c_api) == TK_AT)
          {
            // We have an actor marked as C-API.
            ast_t* id = ast_child(entity);

            // Generate the actor.
            gentype_t g;
            ast_t* ast = genprim(c, entity, ast_name(id), &g);

            if(ast == NULL)
              return false;

            found = true;
          }
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }

  if(!found)
  {
    errorf(NULL, "no C-API actors found in package '%s'", c->filename);
    return false;
  }

  return true;
}

static bool link_lib(compile_t* c, pass_opt_t* opt, const char* file_o)
{
  size_t len = strlen(c->filename);

#if defined(PLATFORM_IS_POSIX_BASED)
  VLA(char, libname, len + 4);
  memcpy(libname, "lib", 3);
  memcpy(libname + 3, c->filename, len + 1);

  const char* file_lib = suffix_filename(opt->output, libname, ".a");
  printf("Archiving %s\n", file_lib);

  len = 32 + strlen(file_lib) + strlen(file_o);
  VLA(char, cmd, len);

  snprintf(cmd, len, "ar -rcs %s %s", file_lib, file_o);

  if(system(cmd) != 0)
  {
    errorf(NULL, "unable to link");
    return false;
  }
#elif defined(PLATFORM_IS_WINDOWS)
  VLA(char, libname, len + 1);
  memcpy(libname, c->filename, len + 1);

  const char* file_lib = suffix_filename(opt->output, libname, ".lib");
  printf("Archiving %s\n", file_lib);

  vcvars_t vcvars;

  if(!vcvars_get(&vcvars))
  {
    errorf(NULL, "unable to link");
    return false;
  }

  len = 32 + strlen(file_lib) + strlen(file_o);
  VLA(char, cmd, len);

  snprintf(cmd, len, " /NOLOGO /OUT:%s %s", file_lib, file_o);

  if(!system(vcvars.ar, cmd))
  {
    errorf(NULL, "unable to link");
    return false;
  }
#endif

  return true;
}

bool genlib(compile_t* c, pass_opt_t* opt, ast_t* program)
{
  // Open a header file.
  const char* file_h = suffix_filename(opt->output, c->filename, ".h");
  c->header = fopen(file_h, "wt");

  if(c->header == NULL)
  {
    errorf(NULL, "couldn't write to %s", file_h);
    return false;
  }

  fprintf(c->header,
    "#ifndef pony_%s_h\n"
    "#define pony_%s_h\n"
    "\n"
    "/* This is an auto-generated header file. Do not edit. */\n"
    "\n"
    "#include <stdint.h>\n"
    "#include <stdbool.h>\n"
    "\n"
    "#ifdef __cplusplus\n"
    "extern \"C\" {\n"
    "#endif\n"
    "\n"
    "#ifdef _MSC_VER\n"
    "typedef struct __int128_t { uint64_t low; int64_t high; } __int128_t;\n"
    "typedef struct __uint128_t { uint64_t low; uint64_t high; } __uint128_t;\n"
    "#endif\n"
    "\n",
    c->filename,
    c->filename
    );

  c->header_buf = printbuf_new();
  bool ok = generate_actors(c, program);

  fwrite(c->header_buf->m, 1, c->header_buf->offset, c->header);
  printbuf_free(c->header_buf);
  c->header_buf = NULL;

  fprintf(c->header,
    "\n"
    "#ifdef __cplusplus\n"
    "}\n"
    "#endif\n"
    "\n"
    "#endif\n"
    );

  fclose(c->header);
  c->header = NULL;

  if(!ok)
  {
    unlink(file_h);
    return false;
  }

  const char* file_o = genobj(c, opt);

  if(file_o == NULL)
    return false;

  if(opt->limit < PASS_ALL)
    return true;

  if(!link_lib(c, opt, file_o))
    return false;

  unlink(file_o);
  return true;
}
