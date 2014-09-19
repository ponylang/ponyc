#include <platform.h>
#include "../ast/error.h"

#if defined(PLATFORM_IS_WINDOWS)

#define REG_SDK_INSTALL_PATH \
  TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\Microsoft SDKs\\Windows\\")
#define REG_VS_INSTALL_PATH \
  TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VS7")

#define MAX_VER_LEN 10

typedef void(*query_callback_fn)(HKEY key, char* name, void* p);
typedef void(*file_search_fn)(char* path, void* p);

typedef struct search_t
{
  char path[MAX_PATH];
  char version[MAX_VER_LEN + 1];
} search_t;

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
  query_callback_fn fn, void* p)
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

static bool find_registry_key(char* path, query_callback_fn query, 
  bool query_subkeys, void* p)
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

static void pick_newest_vs(HKEY key, char* name, void* p)
{
  search_t* vs = (search_t*)p;
  DWORD idx = 0;
  DWORD status = 0;

  DWORD ver_size = MAX_VER_LEN + 1;
  DWORD path_len = MAX_PATH + 1;

  DWORD size = ver_size;

  //thats a bit fragile
  VLA(char, new_version, ver_size);
  VLA(char, new_path, path_len);

  while(true)
  {
    status = RegEnumValue(key, idx, new_version, &size, NULL, NULL, NULL, NULL);
    
    if(status != ERROR_SUCCESS)
      break;
    
    if((strlen(vs->version) == 0)
      || (strncmp(vs->version, new_version, strlen(vs->version)) < 0))
    {
      if(RegGetValue(key, NULL, new_version,
        RRF_RT_REG_SZ, NULL, new_path, &path_len) == ERROR_SUCCESS)
      {
        strcpy(vs->path, new_path);
        strcpy(vs->version, new_version);
      }
    }

    idx++;
    size = ver_size;
  };
}

static void pick_newest_sdk(HKEY key, char* name, void* p)
{
  search_t* sdk = (search_t*)p;

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
        strcpy(sdk->path, new_path);
        strcpy(sdk->version, new_version);
      }
    }
  }
}

/*static bool find_file(char* search_path, char* name, void* p, file_search_fn fn)
{
  bool found = false;

  WIN32_FIND_DATA info;

  TCHAR current[MAX_PATH];
  TCHAR recurse[MAX_PATH];
  
  strcpy(current, search_path);
  strcat(current, "*");
  strcpy(recurse, search_path);

  HANDLE hFound = FindFirstFile(current, &info);

  do
  {
    if(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
      if((strcmp(info.cFileName, ".") == 0) || (strcmp(info.cFileName, "..") == 0))
        continue;

      strcat(recurse, info.cFileName);
      strcat(recurse, "\\");

      found |= find_file(recurse, name, p, fn);

      strcpy(recurse, search_path);
    }
    else
    {
      if(_stricmp(info.cFileName, name) == 0)
      {
        fn(recurse, p);
        found = true;
      }
    }
  } while(FindNextFile(hFound, &info));

  FindClose(hFound);
  
  return found;
}*/

/*static void append_path(char*** pathsv, size_t* idx, char* new_path)
{
  size_t prev = *idx;
  size_t len = strlen(new_path) + 1;
 
  *pathsv = (char**)realloc((*pathsv), sizeof(char*) * (++(*idx)));

  (*pathsv)[prev] = (char*)malloc(sizeof(char) * len);
  memcpy((*pathsv)[prev], new_path, len);
}*/

/*static void kernel32_match(char* path, void* p)
{
  vcvars_t* vcvars = (vcvars_t*)p;
  append_path(&vcvars->kernel32, &vcvars->kernel32_paths, path);
  
  vcvars->length += strlen(path) + 1;
}*/

static bool find_kernel32(vcvars_t* vcvars)
{
  search_t sdk;
  memset(sdk.version, 0, MAX_VER_LEN + 1);

  if(!find_registry_key(REG_SDK_INSTALL_PATH, pick_newest_sdk, true, &sdk))
  {
    errorf(NULL, "unable to locate a Windows SDK");
    return false;
  }

  /*if(!find_file(sdk.path, "kernel32.lib", vcvars, kernel32_match))
  {
    errorf(NULL, "unable to locate kernel32.lib");
    return false;
  }*/

  strcpy(vcvars->kernel32, sdk.path); 
  strcat(vcvars->kernel32, "Lib\\winv6.3\\um\\x64");

  return true;
}

/*static void msvcrt_match(char* path, void* p)
{
  vcvars_t* vcvars = (vcvars_t*)p; 
  append_path(&vcvars->msvcrt, &vcvars->msvcrt_paths, path);
  
  vcvars->length += strlen(path) + 1;
}*/

static bool find_msvcrt(vcvars_t* vcvars)
{
  search_t vs;
  memset(vs.version, 0, MAX_VER_LEN + 1);

  if(!find_registry_key(REG_VS_INSTALL_PATH, pick_newest_vs, false, &vs))
  {
    errorf(NULL, "unable to locate Visual Studio");
    return false;
  }

  /*if(!find_file(vs.path, "msvcrt.lib", vcvars, msvcrt_match))
  {
    errorf(NULL, "unable to locate msvcrt.lib");
    return false;
  }*/

  strcpy(vcvars->msvcrt, vs.path);
  strcat(vcvars->msvcrt, "VC\\lib\\amd64");

  //find the linker relative to vs.path
  //we expect one to be at vs.path\bin
  TCHAR linker_path[MAX_PATH + 1];
  strcpy(linker_path, vs.path);
  strcat(linker_path, "VC\\bin\\link.exe");

  if((GetFileAttributes(linker_path) != INVALID_FILE_ATTRIBUTES))
  {
    strcpy(vcvars->link, linker_path);
  }
  else
  {
    errorf(NULL, "unable to locate link.exe");
    return false;
  }

  return true;
}

bool vcvars_get(vcvars_t* vcvars)
{
  //vcvars->kernel32 = NULL;
  //vcvars->msvcrt = NULL;
  //vcvars->kernel32_paths = 0;
  //vcvars->msvcrt_paths = 0;
  //vcvars->length = 0;

  if(!find_kernel32(vcvars))
    return false;

  if(!find_msvcrt(vcvars))
    return false;
  
  return true;
}

size_t vcvars_get_path_length(vcvars_t* vcvars)
{
  //space seperated paths
  return (2 * ((MAX_PATH + 1) + 11));
}

#endif
