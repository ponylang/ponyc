#include <platform.h>
#include "../ast/error.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <stdlib.h>

#if defined(PLATFORM_IS_WINDOWS)

#define REG_SDK_INSTALL_PATH \
  TEXT("SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\")
#define REG_VS_INSTALL_PATH \
  TEXT("SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\")

#define MAX_VER_LEN 20

typedef struct search_t search_t;
typedef void(*query_callback_fn)(HKEY key, char* name, search_t* p);

struct search_t
{
  long latest_ver;
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

static void pick_vc_tools(HKEY key, char* name, search_t* p)
{
  DWORD size = MAX_PATH;

  RegGetValue(key, NULL, "InstallDir", RRF_RT_REG_SZ,
    NULL, p->path, &size);
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
    char* last_dot = strrchr(new_version, '.');
    if(last_dot == NULL)
      return;

    long new_ver = atol(last_dot + 1);

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

  strcpy(vcvars->kernel32, sdk.path);
  strcat(vcvars->kernel32, "Lib\\");
  if(strcmp("v8.0", sdk.name) == 0)
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
    strcat(vcvars->kernel32, ".0");
  }
  else
  {
    return false;
  }
  strcat(vcvars->kernel32, "\\um\\x64");

  return true;
}

static bool find_executable(const char* path, const char* name, char* dest,
  errors_t* errors)
{
  TCHAR exe[MAX_PATH + 1];
  strcpy(exe, path);
  strcat(exe, name);

  if((GetFileAttributes(exe) == INVALID_FILE_ATTRIBUTES))
  {
    errorf(errors, NULL, "unable to locate %s", name);
    return false;
  }

  strcpy(dest, exe);
  return true;
}

static bool find_msvcrt_and_linker(vcvars_t* vcvars, errors_t* errors)
{
  search_t vs;

  snprintf(vs.version, sizeof(vs.version), "%d.%d",
    PLATFORM_TOOLS_VERSION / 10,
    PLATFORM_TOOLS_VERSION % 10);

  TCHAR reg_vs_install_path[MAX_PATH+1];
  snprintf(reg_vs_install_path, MAX_PATH, "%s%s", REG_VS_INSTALL_PATH, vs.version);

  if(!find_registry_key(reg_vs_install_path, pick_vc_tools, false, &vs))
  {
    errorf(errors, NULL, "unable to locate Visual Studio %s", vs.version);
    return false;
  }

  strcpy(vcvars->msvcrt, vs.path);
  strcat(vcvars->msvcrt, "..\\..\\VC\\lib\\amd64");

  // Find the linker and lib archiver relative to vs.path.
  return
    find_executable(vs.path, "..\\..\\VC\\bin\\link.exe", vcvars->link, errors) &&
    find_executable(vs.path, "..\\..\\VC\\bin\\lib.exe", vcvars->ar, errors);
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
