#include <platform.h>
#include "../ast/error.h"
#include "../../libponyrt/mem/pool.h"

#if defined(PLATFORM_IS_WINDOWS)

#define REG_SDK_INSTALL_PATH \
  TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\Microsoft SDKs\\Windows\\")
#define REG_VS_INSTALL_PATH \
  TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VS7")
#define REG_VC_TOOLS_PATH \
  TEXT("SOFTWARE\\Microsoft\\DevDiv\\VCForPython\\9.0")

#define MAX_VER_LEN 10

typedef struct search_t search_t;
typedef void(*query_callback_fn)(HKEY key, char* name, search_t* p);

struct search_t
{
  char path[MAX_PATH + 1];
  char version[MAX_VER_LEN + 1];
};

void get_child_count(HKEY key, DWORD* count, DWORD* largest_subkey)
{
  if(RegQueryInfoKey(key, NULL, NULL, NULL,
    count, largest_subkey, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
  {
    *count = 0;
    *largest_subkey = 0;
  }
}

bool query_registry(HKEY key, bool query_subkeys,
  query_callback_fn fn, search_t* p)
{
  DWORD sub_keys;
  DWORD largest_subkey;

  //Processing a leaf node in the registry, give it
  //to the callback.
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
  char* name = (char*)pool_alloc_size(largest_subkey);

  for(DWORD i = 0; i < sub_keys; ++i)
  {
    if(RegEnumKeyEx(key, i, name, &size,
      NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
    {
      if(RegOpenKeyEx(key, name, 0, KEY_QUERY_VALUE, &node) == ERROR_SUCCESS)
      {
        fn(node, name, p);
      }
      else
      {
        pool_free_size(largest_subkey, name);
        return false;
      }

      RegCloseKey(node);
    }
    else
    {
      pool_free_size(largest_subkey, name);
      return false;
    }

    size = largest_subkey;
  }

  pool_free_size(largest_subkey, name);
  return true;
}

static bool find_registry_key(char* path, query_callback_fn query,
  bool query_subkeys, search_t* p)
{
  bool success = true;
  HKEY key;

  //try global installations
  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0,
    (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) != ERROR_SUCCESS)
  {
    //try user-only installations
    if(RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,
      (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) != ERROR_SUCCESS)
    {
      success = false;
    }
  }

  if(success)
  {
    if(!query_registry(key, query_subkeys, query, p))
      success = false;
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

static void pick_vs_tools(HKEY key, char* name, search_t* p)
{
  DWORD size = MAX_PATH;

  RegGetValue(key, NULL, p->version, RRF_RT_REG_SZ, NULL, p->path, &size);
}

static void pick_newest_sdk(HKEY key, char* name, search_t* p)
{
  //it seems to be the case that sub nodes ending
  //with an 'A' are .NET Framework SDKs
  if(name[strlen(name) - 1] == 'A')
    return;

  DWORD path_len = MAX_PATH;
  DWORD version_len = MAX_VER_LEN;
  char new_path[MAX_PATH];

  if(RegGetValue(key, NULL, "InstallationFolder", RRF_RT_REG_SZ,
    NULL, new_path, &path_len) == ERROR_SUCCESS)
  {
    char new_version[MAX_VER_LEN];

    if(RegGetValue(key, NULL, "ProductVersion",
      RRF_RT_REG_SZ, NULL, new_version, &version_len) == ERROR_SUCCESS)
    {
      if((strlen(p->version) == 0)
        || (strncmp(p->version, new_version, strlen(p->version)) < 0))
      {
        strcpy(p->path, new_path);
        strcpy(p->version, new_version);
      }
    }
  }
}

static bool find_kernel32(vcvars_t* vcvars)
{
  search_t sdk;
  memset(&sdk, 0, sizeof(search_t));

  if(!find_registry_key(REG_SDK_INSTALL_PATH, pick_newest_sdk, true, &sdk))
  {
    errorf(NULL, "unable to locate a Windows SDK");
    return false;
  }

  strcpy(vcvars->kernel32, sdk.path);
  strcat(vcvars->kernel32, "Lib\\winv6.3\\um\\x64");

  return true;
}

static bool find_executable(const char* path, const char* name, char* dest)
{
  TCHAR exe[MAX_PATH + 1];
  strcpy(exe, path);
  strcat(exe, name);

  if((GetFileAttributes(exe) != INVALID_FILE_ATTRIBUTES))
  {
    strcpy(dest, exe);
    return true;
  }
  else
  {
    errorf(NULL, "unable to locate %s", name);
    return false;
  }
}

static bool find_msvcrt_and_linker(vcvars_t* vcvars)
{
  search_t vs;

  snprintf(vs.version, sizeof(vs.version), "%d.%d",
    PLATFORM_TOOLS_VERSION / 10,
    PLATFORM_TOOLS_VERSION % 10);

  if(!find_registry_key(REG_VC_TOOLS_PATH, pick_vc_tools, false, &vs) &&
    !find_registry_key(REG_VS_INSTALL_PATH, pick_vs_tools, false, &vs))
  {
    errorf(NULL, "unable to locate VC tools or Visual Studio %s", vs.version);
    return false;
  }

  strcpy(vcvars->msvcrt, vs.path);
  strcat(vcvars->msvcrt, "VC\\lib\\amd64");

  //Find the linker relative to vs.path. We expect one to be at vs.path\bin.
  //The same applies to the lib archiver.
  bool ok = find_executable(vs.path, "VC\\bin\\link.exe", vcvars->link);
  ok &= find_executable(vs.path, "VC\\bin\\lib.exe", vcvars->ar);

  return ok;
}

bool vcvars_get(vcvars_t* vcvars)
{
  if(!find_kernel32(vcvars))
    return false;

  if(!find_msvcrt_and_linker(vcvars))
    return false;

  return true;
}

#endif
