#include <platform.h>
#include "../ast/error.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <stdlib.h>

#if defined(PLATFORM_IS_WINDOWS)

#define REG_SDK_INSTALL_PATH \
  TEXT("SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\")
#define REG_VS_INSTALL_PATH \
  TEXT("SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\SxS\\VS7")

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
    "kernel32.lib msvcrt.lib Ws2_32.lib advapi32.lib vcruntime.lib "
    "legacy_stdio_definitions.lib dbghelp.lib");

  strcpy(vcvars->kernel32, sdk.path);
  strcat(vcvars->kernel32, "Lib\\");
  if(strcmp("v7.1", sdk.name) == 0)
  {
    strcat(vcvars->kernel32 , "\\x64");
    return true;
  }
  else if(strcmp("v8.0", sdk.name) == 0)
  {
    strcat(vcvars->kernel32 , "win8");
  }
  else if(strcmp("v8.1", sdk.name) == 0)
  {
    strcat(vcvars->kernel32, "winv6.3");
  }
  else if(strcmp("v10.0", sdk.name) == 0)
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
    strcpy(dest, full_path);
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

        strcpy(full_path, path);
        strcat(full_path, entry);
        if ((GetFileAttributes(full_path) != FILE_ATTRIBUTE_DIRECTORY))
          continue;
        
        uint64_t ver = get_version(entry);
        if (ver == 0)
          continue;
        
        if (ver > best_version)
        {
          best_version = ver;
          strcpy(best_path, full_path);
          strcat(best_path, "\\");
        }
      }

      pony_closedir(dir);

      if (best_version > 0)
        return find_executable(best_path, name, dest, true, errors);
    }
  }

  return false;
}

static bool find_msvcrt_and_linker(vcvars_t* vcvars, errors_t* errors)
{
  search_t vs;

  snprintf(vs.version, sizeof(vs.version), "%d.%d",
    PLATFORM_TOOLS_VERSION / 10,
    PLATFORM_TOOLS_VERSION % 10);

  TCHAR reg_vs_install_path[MAX_PATH+1];
  snprintf(reg_vs_install_path, MAX_PATH, "%s", REG_VS_INSTALL_PATH);

  if(!find_registry_value(reg_vs_install_path, vs.version, &vs))
  {
    errorf(errors, NULL, "unable to locate Visual Studio %s", vs.version);
    return false;
  }

  // Find the linker and lib archiver relative to vs.path (for VS2015 and earlier)
  if (find_executable(vs.path, "VC\\bin\\amd64\\link.exe", vcvars->link, false, errors) &&
      find_executable(vs.path, "VC\\bin\\amd64\\lib.exe", vcvars->ar, false, errors))
  {
    strcpy(vcvars->msvcrt, vs.path);
    strcat(vcvars->msvcrt, "VC\\lib\\amd64");
    return true;
  }

  // Find the linker and lib archiver for VS2017
  strcat(vs.path, "VC\\Tools\\MSVC\\");
  if (find_executable(vs.path, "bin\\HostX64\\x64\\link.exe", vcvars->link, true, errors))
  {
    strcpy(vs.path, vcvars->link);
    vs.path[strlen(vcvars->link)-24] = 0;
    if (find_executable(vs.path, "bin\\HostX64\\x64\\lib.exe", vcvars->ar, false, errors))
    {
      strcpy(vcvars->msvcrt, vs.path);
      strcat(vcvars->msvcrt, "lib\\x64");
      return true;
    }
  }

  errorf(errors, NULL, "unable to locate Visual Studio %s link.exe", vs.version);
  return false;
}

bool vcvars_get(vcvars_t* vcvars, errors_t* errors)
{
  if(!find_kernel32(vcvars, errors))
    return false;

  if(!find_msvcrt_and_linker(vcvars, errors))
    return false;

  return true;
}

#endif
