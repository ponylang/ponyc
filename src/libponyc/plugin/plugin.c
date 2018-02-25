#include "plugin.h"
#include "../ast/error.h"
#include "../pass/pass.h"
#include "../pkg/package.h"
#include "../../libponyrt/ds/list.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

#ifdef PLATFORM_IS_POSIX_BASED
#  include <dlfcn.h>
#  define PLUGIN_LOAD(path) dlopen(path, RTLD_LAZY)
#  define PLUGIN_UNLOAD(handle) dlclose(handle)
#  define PLUGIN_SYMBOL(handle, name) dlsym(handle, name)

typedef void* plugin_handle_t;

#elif defined(PLATFORM_IS_WINDOWS)
#  include <Windows.h>
#  define PLUGIN_LOAD(path) LoadLibrary(path)
#  define PLUGIN_UNLOAD(handle) FreeLibrary(handle)
#  define PLUGIN_SYMBOL(handle, name) GetProcAddress(handle, name)

typedef HMODULE plugin_handle_t;

#endif

typedef bool(*plugin_init_fn)(pass_opt_t* opt, void** user_data);
typedef void(*plugin_final_fn)(pass_opt_t* opt, void* user_data);
typedef const char*(*plugin_help_fn)(void* user_data);
typedef bool(*plugin_parse_options_fn)(pass_opt_t* opt, int* argc, char** argv,
  void* user_data);
typedef void(*plugin_visit_ast_fn)(const ast_t* ast, pass_opt_t* opt,
  pass_id pass, void* user_data);
typedef void(*plugin_visit_reach_fn)(const reach_t* r, pass_opt_t* opt,
  bool built_vtables, void* user_data);
typedef void(*plugin_visit_compile_fn)(const compile_t* c, pass_opt_t* opt,
  void* user_data);

typedef struct plugin_t
{
  plugin_handle_t handle;
  const char* path;

  plugin_init_fn init_fn;
  plugin_final_fn final_fn;
  plugin_help_fn help_fn;
  plugin_parse_options_fn parse_options_fn;
  plugin_visit_ast_fn visit_ast_fn;
  plugin_visit_reach_fn visit_reach_fn;
  plugin_visit_compile_fn visit_compile_fn;

  void* user_data;
} plugin_t;

static __pony_thread_local pass_opt_t* opt_for_free = NULL;

static void plugin_free(plugin_t* p)
{
  pony_assert(opt_for_free != NULL);

  if(p->final_fn != NULL)
    p->final_fn(opt_for_free, p->user_data);

  PLUGIN_UNLOAD(p->handle);
  POOL_FREE(plugin_t, p);
}

DECLARE_LIST(plugins, plugins_t, plugin_t);
DEFINE_LIST(plugins, plugins_t, plugin_t, NULL, plugin_free);

static bool load_plugin(const char* path, pass_opt_t* opt)
{
  plugin_handle_t handle = PLUGIN_LOAD(path);

  if(handle == NULL)
  {
    errorf(opt->check.errors, NULL, "Couldn't find plugin %s", path);
    return false;
  }

  plugin_t* p = POOL_ALLOC(plugin_t);
  p->handle = handle;
  p->path = stringtab(path);
  p->user_data = NULL;

  p->init_fn = (plugin_init_fn)PLUGIN_SYMBOL(handle, "pony_plugin_init");
  p->final_fn = (plugin_final_fn)PLUGIN_SYMBOL(handle, "pony_plugin_final");
  p->help_fn = (plugin_help_fn)PLUGIN_SYMBOL(handle, "pony_plugin_help");
  p->parse_options_fn = (plugin_parse_options_fn)PLUGIN_SYMBOL(handle,
    "pony_plugin_parse_options");
  p->visit_ast_fn = (plugin_visit_ast_fn)PLUGIN_SYMBOL(handle,
    "pony_plugin_visit_ast");
  p->visit_reach_fn = (plugin_visit_reach_fn)PLUGIN_SYMBOL(handle,
    "pony_plugin_visit_reach");
  p->visit_compile_fn = (plugin_visit_compile_fn)PLUGIN_SYMBOL(handle,
    "pony_plugin_visit_compile");

  if((p->init_fn != NULL) && !p->init_fn(opt, &p->user_data))
  {
    PLUGIN_UNLOAD(p->handle);
    POOL_FREE(plugin_t, p);
    return false;
  }

  opt->plugins = plugins_append(opt->plugins, p);

  return true;
}

bool plugin_load(pass_opt_t* opt, const char* paths)
{
  return handle_path_list(paths, load_plugin, opt);
}

void plugin_print_help(pass_opt_t* opt)
{
  plugins_t* plugins = opt->plugins;

  while(plugins != NULL)
  {
    plugin_t* plugin = plugins_data(plugins);

    if(plugin->help_fn != NULL)
    {
      const char* str = plugin->help_fn(plugin->user_data);
      printf("\nHelp for plugin %s:\n%s\n", plugin->path, str);
    } else {
      printf("\nNo help for plugin %s\n", plugin->path);
    }

    plugins = plugins_next(plugins);
  }
}

bool plugin_parse_options(pass_opt_t* opt, int* argc, char** argv)
{
  bool ok = true;
  plugins_t* plugins = opt->plugins;

  while(plugins != NULL)
  {
    plugin_t* plugin = plugins_data(plugins);

    if(plugin->parse_options_fn != NULL)
    {
      if(!plugin->parse_options_fn(opt, argc, argv, plugin->user_data))
        ok = false;
    }

    plugins = plugins_next(plugins);
  }

  return ok;
}

void plugin_visit_ast(const ast_t* ast, pass_opt_t* opt, pass_id pass)
{
  plugins_t* plugins = opt->plugins;

  while(plugins != NULL)
  {
    plugin_t* plugin = plugins_data(plugins);

    if(plugin->visit_ast_fn != NULL)
      plugin->visit_ast_fn(ast, opt, pass, plugin->user_data);

    plugins = plugins_next(plugins);
  }
}

void plugin_visit_reach(const reach_t* r, pass_opt_t* opt, bool built_vtables)
{
  plugins_t* plugins = opt->plugins;

  while(plugins != NULL)
  {
    plugin_t* plugin = plugins_data(plugins);

    if(plugin->visit_reach_fn != NULL)
      plugin->visit_reach_fn(r, opt, built_vtables, plugin->user_data);

    plugins = plugins_next(plugins);
  }
}

void plugin_visit_compile(const compile_t* c, pass_opt_t* opt)
{
  plugins_t* plugins = opt->plugins;

  while(plugins != NULL)
  {
    plugin_t* plugin = plugins_data(plugins);

    if(plugin->visit_compile_fn != NULL)
      plugin->visit_compile_fn(c, opt, plugin->user_data);

    plugins = plugins_next(plugins);
  }
}

void plugin_unload(pass_opt_t* opt)
{
  opt_for_free = opt;

  plugins_free(opt->plugins);
  opt->plugins = NULL;

  opt_for_free = NULL;
}
