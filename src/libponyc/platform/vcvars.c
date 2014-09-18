#include <platform/platform.h>

#if defined(PLATFORM_IS_WINDOWS)

#define REG_SDK_INSTALL_PATH \
  TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\Microsoft SDKs\\Windows\\")
#define REG_SDK_INSTALL_PATH_FALLBACK \
  TEXT("SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\")

#define MAX_VER_LEN 10

typedef void(*query_callback_fn)(HKEY key, char* name, void* p);

typedef struct sdk_search_t
{
  vcvars_t* vcvars;
  char version[MAX_VER_LEN + 1];
} sdk_search_t;

void get_child_count(HKEY key, DWORD* count, DWORD* largest_subkey)
{
  if(RegQueryInfoKey(key, NULL, NULL, NULL,
    count, largest_subkey, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
  {
    *count = 0;
    *largest_subkey = 0;
  }
}

bool query_registry(HKEY key, query_callback_fn fn, void* p)
{
  DWORD sub_keys;
  DWORD largest_subkey;

  get_child_count(key, &sub_keys, &largest_subkey);

  if(sub_keys == 0)
    return false;

  largest_subkey += 1;

  HKEY node;
  DWORD size = largest_subkey;
  VLA(char, name, largest_subkey);

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
        return false;
      }

      RegCloseKey(node);
    }
    else
    {
      return false;
    }

    size = largest_subkey;
  }

  return true;
}

static void pick_newest_sdk(HKEY key, char* name, void* p)
{
  sdk_search_t* sdk = (sdk_search_t*)p;

  //it seesm to be the case that sub nodes ending
  //with an 'A' are .NET Framework SDKs
  if(name[strlen(name) - 1] == 'A')
    return;

  DWORD path_len = MAX_PATH + 1;
  DWORD version_len = MAX_VER_LEN + 1;
  VLA(char, new_path, path_len);

  if(RegGetValue(key, NULL, "InstallationFolder", RRF_RT_REG_SZ,
    NULL, new_path, &path_len) == ERROR_SUCCESS)
  {
    VLA(char, new_version, version_len);

    if(RegGetValue(key, NULL, "ProductVersion",
      RRF_RT_REG_SZ, NULL, new_version, &version_len) == ERROR_SUCCESS)
    {
      if((strlen(sdk->version) == 0)
        || (strncmp(sdk->version, new_version, strlen(sdk->version)) < 0))
      {
        memcpy(sdk->vcvars->sdk_lib_dir, new_path, MAX_PATH + 1);
        memcpy(sdk->version, new_version, version_len);
      }
    }
  }
}

static bool find_kernel32(sdk_search_t* search)
{
  bool success = true;
  HKEY key;

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_SDK_INSTALL_PATH, 0,
    (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) != ERROR_SUCCESS)
  {
    //try the fallback, otherwise give up
    if(RegOpenKeyEx(HKEY_CURRENT_USER, REG_SDK_INSTALL_PATH_FALLBACK, 0,
      (KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE), &key) != ERROR_SUCCESS)
    {
      success = false;
    }
  }

  if(success)
  {
    if(!query_registry(key, pick_newest_sdk, search))
      success = false;
  }

  RegCloseKey(key);

  //we try to search for the 64-bit version of kernel32.lib provided with
  //the Windows SDK framework we have just found.
  //strcat(search->vcvars->sdk_lib_dir, "Lib\\winv6.3\\um\\x64");

  return success;
}

static bool find_msvcrt(vcvars_t* vcvars)
{
  return false;
}

bool vcvars_get(vcvars_t* vcvars)
{
  memset(vcvars->sdk_lib_dir, 0, MAX_PATH + 1);

  sdk_search_t sdk;
  sdk.vcvars = vcvars;

  memset(sdk.version, 0, MAX_VER_LEN + 1);

  if(!find_kernel32(&sdk))
    return false;

  if(!find_msvcrt(vcvars))
    return false;

  return true;
}

#endif
