#include <platform.h>
#include "../ast/error.h"
#include "../codegen/codegen.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <stdlib.h>

#if defined(PLATFORM_IS_WINDOWS)

#define REG_SDK_INSTALL_PATH \
  TEXT("SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\")

typedef struct vsinfo_t
{
  char* version;     // version for informational purposes
  char* reg_path;    // registry path to search
  char* reg_key;     // registry key to search
  char* search_path; // install subdir; must have trailing '\\' if present
  char* bin_path;    // bin subdir; must have trailing '\\'
  char* lib_path;    // lib subdir; must NOT have trailing '\\'
} vsinfo_t;

static vsinfo_t vs_infos[] =
{
  { // VS2017 full install & Visual C++ Build Tools 2017
    "15.0", "SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\SxS\\VS7", 
    "15.0", "VC\\Tools\\MSVC\\", "bin\\HostX64\\x64\\", "lib\\x64" 
  },
  { // VS2015 full install
    "14.0", "SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\SxS\\VS7", 
    "14.0", NULL, "VC\\bin\\amd64\\", "VC\\lib\\amd64" 
  },
  { // Visual C++ Build Tools 2015
    "14.0", "SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\SxS\\VC7", 
    "14.0", NULL, "bin\\amd64\\", "lib\\amd64" 
  },
  { // VS2015 fallback
    "14.0", "SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\14.0", 
    "InstallDir", NULL, "..\\..\\VC\\bin\\amd64\\", "..\\..\\VC\\lib\\amd64" 
  },
  {
    NULL
  }
};

#define MAX_VER_LEN 20

typedef struct search_t search_t;
typedef void(*query_callback_fn)(HKEY key, char* name, search_t* p);

struct search_t
{
  uint64_t latest_ver;
  char name[MAX_VER_LEN + 1];
  char path[MAX_PATH + 1];
  char version[MAX_VER_LEN + 1];
};

static void get_child_count(HKEY key, DWORD* count, DWORD* largest_subkey)
{
  if(RegQueryInfoKey(key, NULL, NULL, NULL, count, largest_subkey, NULL, NULL,
    NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
  {
    *count = 0;
    *largest_subkey = 0;
  }
}

static bool query_registry(HKEY key, bool query_subkeys, query_callback_fn fn,
  search_t* p)
{
  DWORD sub_keys;
  DWORD largest_subkey;

  // Processing a leaf node in the registry, give it to the callback.
  if(!query_subkeys)
  {
    fn(key, NULL, p);
    return true;
  }

  get_child_count(key, &sub_keys, &largest_subkey);

  if(sub_keys == 0)
    return false;

  largest_subkey += 1;

  HKEY node;
  DWORD size = largest_subkey;
  char* name = (char*)ponyint_pool_alloc_size(largest_subkey);
  bool r = true;

  for(DWORD i = 0; i < sub_keys; ++i)
  {
    if(RegEnumKeyEx(key, i, name, &size, NULL, NULL, NULL, NULL)
      != ERROR_SUCCESS ||
      RegOpenKeyEx(key, name, 0, KEY_QUERY_VALUE, &node) != ERROR_SUCCESS)
    {
      r = false;
      break;
    }

    fn(node, name, p);
    RegCloseKey(node);
    size = largest_subkey;
  }

  ponyint_pool_free_size(largest_subkey, name);
  return true;
}

static bool find_registry_key(char* path, query_callback_fn query,
  bool query_subkeys, search_t* p)
{
  bool success = false;
  HKEY key;

  // Try global installations then user-only.
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0,
      (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) == ERROR_SUCCESS ||
    RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,
      (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) == ERROR_SUCCESS)
  {
    success = query_registry(key, query_subkeys, query, p);
  }

  RegCloseKey(key);
  return success;
}

static bool find_registry_value(char *path, char *name, search_t* p)
{
  bool success = false;
  HKEY key;

  // Try global installations then user-only.
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0,
      (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) == ERROR_SUCCESS ||
    RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,
      (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) == ERROR_SUCCESS)
  {
    DWORD size = MAX_PATH;
    success = RegGetValue(key, NULL, name, RRF_RT_REG_SZ, 
      NULL, p->path, &size) == ERROR_SUCCESS;
  }

  RegCloseKey(key);
  return success;  
}

static uint64_t get_version(const char* str)
{
  char tokens[MAX_VER_LEN + 1];
  strncpy(tokens, str, MAX_VER_LEN);
  uint64_t version = 0;
  char *token = tokens;

  while (*token != NULL && !isdigit(*token))
    token++;
  
  if (*token != NULL)
  {
    char *dot;
    int place = 3;
    while (place >= 0)
    {
      if ((dot = strchr(token, '.')) != NULL)
        *dot = '\0';
      uint64_t num = atol(token);
      if (num > 0xffff) num = 0xffff;
      version += (num << (place * 16));

      place--;

      if (dot != NULL)
        token = dot + 1;
      else
        break;
    }
  }

  return version;
}

static void pick_newest_sdk(HKEY key, char* name, search_t* p)
{
  // SDKs ending with 'A' are .NET Framework.
  if(name[strlen(name) - 1] == 'A')
    return;

  DWORD path_len = MAX_PATH;
  DWORD version_len = MAX_VER_LEN;
  char new_path[MAX_PATH];
  char new_version[MAX_VER_LEN];

  if(RegGetValue(key, NULL, "InstallationFolder", RRF_RT_REG_SZ, NULL,
      new_path, &path_len) == ERROR_SUCCESS &&
    RegGetValue(key, NULL, "ProductVersion", RRF_RT_REG_SZ, NULL, new_version,
      &version_len) == ERROR_SUCCESS)
  {
    uint64_t new_ver = get_version(new_version);

    if((strlen(p->version) == 0) || (new_ver > p->latest_ver))
    {
      p->latest_ver = new_ver;
      strcpy(p->path, new_path);
      strcpy(p->version, new_version);
      strncpy(p->name, name, MAX_VER_LEN);
    }
  }
}

static bool find_kernel32(vcvars_t* vcvars, errors_t* errors)
{
  search_t sdk;
  memset(&sdk, 0, sizeof(search_t));

  if(!find_registry_key(REG_SDK_INSTALL_PATH, pick_newest_sdk, true, &sdk))
  {
    errorf(errors, NULL, "unable to locate a Windows SDK");
    return false;
  }

  vcvars->ucrt[0] = '\0';
  strcpy(vcvars->default_libs,
    "kernel32.lib "
#if !defined(DEBUG)
    "msvcrt.lib "
#endif
    "Ws2_32.lib advapi32.lib vcruntime.lib "
    "legacy_stdio_definitions.lib dbghelp.lib");

  strcpy(vcvars->kernel32, sdk.path);
  strcat(vcvars->kernel32, "Lib\\");
  if(strcmp("v10.0", sdk.name) == 0)
  {
    strcat(vcvars->kernel32, sdk.version);

    // recent Windows 10 kits have the correct 4-place version
    // only append ".0" if it doesn't already have it
    size_t len = strlen(vcvars->kernel32);
    if (len > 2 &&
      !(vcvars->kernel32[len - 2] == '.'
        && vcvars->kernel32[len - 1] == '0'))
    {
      strcat(vcvars->kernel32, ".0");
    }

    strcpy(vcvars->ucrt, vcvars->kernel32);
    strcat(vcvars->ucrt, "\\ucrt\\x64");
    strcat(vcvars->default_libs, " ucrt.lib");
  }
  else
  {
    errorf(errors, NULL, "ponyc requires a Windows 10 SDK; "
      "see https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk");
    return false;
  }
  strcat(vcvars->kernel32, "\\um\\x64");

  return true;
}

static bool find_executable(const char* path, const char* name, 
  char* dest, bool recurse, errors_t* errors)
{
  TCHAR full_path[MAX_PATH + 1]; 
  TCHAR best_path[MAX_PATH + 1];
  strcpy(full_path, path);
  strcat(full_path, name);

  if((GetFileAttributes(full_path) != INVALID_FILE_ATTRIBUTES))
  {
    strncpy(dest, full_path, MAX_PATH);
    return true;
  }

  if (recurse)
  {
    PONY_ERRNO err;
    PONY_DIR* dir = pony_opendir(path, &err);
    if (dir != NULL)
    {
      uint64_t best_version = 0;

      // look for directories with versions
      PONY_DIRINFO* result;
      while ((result = pony_dir_entry_next(dir)) != NULL)
      {
        char *entry = pony_dir_info_name(result);
        if (entry == NULL || entry[0] == NULL || entry[0] == '.')
          continue;

        strncpy(full_path, path, MAX_PATH);
        strncat(full_path, entry, MAX_PATH - strlen(full_path));
        if ((GetFileAttributes(full_path) != FILE_ATTRIBUTE_DIRECTORY))
          continue;
        
        uint64_t ver = get_version(entry);
        if (ver == 0)
          continue;
        
        if (ver > best_version)
        {
          best_version = ver;
          strncpy(best_path, full_path, MAX_PATH);
          strncat(best_path, "\\", MAX_PATH - strlen(best_path));
        }
      }

      pony_closedir(dir);

      if (best_version > 0)
        return find_executable(best_path, name, dest, true, errors);
    }
  }

  return false;
}

static bool find_msvcrt_and_linker(compile_t *c, vcvars_t* vcvars, errors_t* errors)
{
  search_t vs;

  TCHAR link_path[MAX_PATH + 1];
  TCHAR lib_path[MAX_PATH + 1];

  vsinfo_t* info;
  for (info = vs_infos; info->version != NULL; info++)
  {
    if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
      fprintf(stderr, "searching for VS in registry: %s\\%s\n", 
        info->reg_path, info->reg_key);

    if (!find_registry_value(info->reg_path, info->reg_key, &vs))
      continue;
    
    strncpy(link_path, info->bin_path, MAX_PATH);
    strncat(link_path, "link.exe", MAX_PATH - strlen(link_path));
    strncpy(lib_path, info->bin_path, MAX_PATH);
    strncat(lib_path, "lib.exe", MAX_PATH - strlen(lib_path));

    // VS2017 may have multiple VC++ installs; search for the latest one
    if (info->search_path != NULL)
    {
      strncat(vs.path, info->search_path, MAX_PATH - strlen(vs.path));
      if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
        fprintf(stderr, "searching for %s .. \\%s and \\%s\n", 
          vs.path, link_path, lib_path);

      if (find_executable(vs.path, link_path, vcvars->link, true, errors))
      {
        strncpy(vs.path, vcvars->link, MAX_PATH);
        size_t ver_index = strlen(vcvars->link) - strlen(link_path);
        strncpy(vs.version, vs.path + ver_index, MAX_PATH);
        vs.path[ver_index] = 0;
        
        if (find_executable(vs.path, lib_path, vcvars->ar, false, errors))
        {
          strncpy(vcvars->msvcrt, vs.path, MAX_PATH);
          strncat(vcvars->msvcrt, info->lib_path, 
            MAX_PATH - strlen(vcvars->msvcrt));
          
          if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
          {
            fprintf(stderr, "linker:  %s\n", vcvars->link);
            fprintf(stderr, "libtool: %s\n", vcvars->ar);
            fprintf(stderr, "libdir:  %s\n", vcvars->msvcrt);
          }

          return true;
        }
      }
    }
    else // VS2015 has one place for VC++
    {      
      if (find_executable(vs.path, link_path, vcvars->link, false, errors) &&
          find_executable(vs.path, lib_path, vcvars->ar, false, errors))
      {
        strncpy(vs.version, info->version, MAX_VER_LEN);

        strncpy(vcvars->msvcrt, vs.path, MAX_PATH);
        strncat(vcvars->msvcrt, info->lib_path, 
          MAX_PATH - strlen(vcvars->msvcrt));

        if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
        {
          fprintf(stderr, "linker:  %s\n", vcvars->link);
          fprintf(stderr, "libtool: %s\n", vcvars->ar);
          fprintf(stderr, "libdir:  %s\n", vcvars->msvcrt);
        }

        return true;
      }
    }
  }
  
  errorf(errors, NULL, "unable to locate a Microsoft link.exe; please "
    "install Visual Studio 2015 or later: https://www.visualstudio.com/");
  return false;
}

bool vcvars_get(compile_t *c, vcvars_t* vcvars, errors_t* errors)
{
  if(!find_kernel32(vcvars, errors))
    return false;

  if(!find_msvcrt_and_linker(c, vcvars, errors))
    return false;

  return true;
}

#endif
