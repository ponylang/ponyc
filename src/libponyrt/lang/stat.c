#include <platform.h>
#include <pony.h>
#include "lang.h"

typedef struct pony_mode_t
{
  pony_type_t* desc;
  bool setuid;
  bool setgid;
  bool sticky;
  bool owner_read;
  bool owner_write;
  bool owner_exec;
  bool group_read;
  bool group_write;
  bool group_exec;
  bool any_read;
  bool any_write;
  bool any_exec;
} pony_mode_t;

typedef struct pony_stat_t
{
  pony_type_t* desc;

  void* path;
  pony_mode_t* mode;

  uint32_t hard_links;
  uint32_t uid;
  uint32_t gid;
  uint64_t size;
  int64_t access_time;
  int64_t access_time_nsec;
  int64_t modified_time;
  int64_t modified_time_nsec;
  int64_t change_time;
  int64_t change_time_nsec;

  bool file;
  bool directory;
  bool pipe;
  bool symlink;
} pony_stat_t;

PONY_EXTERN_C_BEGIN

bool os_stat(const char* path, pony_stat_t* p)
{
#if defined(PLATFORM_IS_WINDOWS)
  struct __stat64 st;

  if(_stat64(path, &st) != 0)
    return false;

  p->file = (st.st_mode & _S_IFREG) == _S_IFREG;
  p->directory = (st.st_mode & _S_IFDIR) == _S_IFDIR;
  p->pipe = (st.st_mode & _S_IFIFO) == _S_IFIFO;
  p->symlink = false;

  p->mode->setuid = false;
  p->mode->setgid = false;
  p->mode->sticky = false;

  p->mode->owner_read = (st.st_mode & _S_IREAD) == _S_IREAD;
  p->mode->owner_write = (st.st_mode & _S_IWRITE) == _S_IWRITE;
  p->mode->owner_exec = (st.st_mode & _S_IEXEC) == _S_IEXEC;

  p->mode->group_read = (st.st_mode & _S_IREAD) == _S_IREAD;
  p->mode->group_write = (st.st_mode & _S_IWRITE) == _S_IWRITE;
  p->mode->group_exec = (st.st_mode & _S_IEXEC) == _S_IEXEC;

  p->mode->any_read = (st.st_mode & _S_IREAD) == _S_IREAD;
  p->mode->any_write = (st.st_mode & _S_IWRITE) == _S_IWRITE;
  p->mode->any_exec = (st.st_mode & _S_IEXEC) == _S_IEXEC;
#elif defined(PLATFORM_IS_POSIX_BASED)
  struct stat st;

  if(lstat(path, &st) != 0)
    return false;

  p->symlink = S_ISLNK(st.st_mode) != 0;

  if(p->symlink)
  {
    if(stat(path, &st) != 0)
      return false;
  }

  p->file = S_ISREG(st.st_mode) != 0;
  p->directory = S_ISDIR(st.st_mode) != 0;
  p->pipe = S_ISFIFO(st.st_mode) != 0;

  p->mode->setuid = (st.st_mode & S_ISUID) != 0;
  p->mode->setgid = (st.st_mode & S_ISGID) != 0;
  p->mode->sticky = (st.st_mode & S_ISVTX) != 0;

  p->mode->owner_read = (st.st_mode & S_IRUSR) != 0;
  p->mode->owner_write = (st.st_mode & S_IWUSR) != 0;
  p->mode->owner_exec = (st.st_mode & S_IXUSR) != 0;

  p->mode->group_read = (st.st_mode & S_IRGRP) != 0;
  p->mode->group_write = (st.st_mode & S_IWGRP) != 0;
  p->mode->group_exec = (st.st_mode & S_IXGRP) != 0;

  p->mode->any_read = (st.st_mode & S_IROTH) != 0;
  p->mode->any_write = (st.st_mode & S_IWOTH) != 0;
  p->mode->any_exec = (st.st_mode & S_IXOTH) != 0;
#endif

  p->hard_links = (uint32_t)st.st_nlink;
  p->uid = st.st_uid;
  p->gid = st.st_gid;
  p->size = st.st_size;
  p->access_time = st.st_atime;
  p->modified_time = st.st_mtime;
  p->change_time = st.st_ctime;

#if defined(PLATFORM_IS_LINUX)
  p->access_time_nsec = st.st_atim.tv_nsec;
  p->modified_time_nsec = st.st_mtim.tv_nsec;
  p->change_time_nsec = st.st_ctim.tv_nsec;
#elif defined(PLATFORM_IS_MACOSX)
  p->access_time_nsec = st.st_atimespec.tv_nsec;
  p->modified_time_nsec = st.st_mtimespec.tv_nsec;
  p->change_time_nsec = st.st_ctimespec.tv_nsec;
#elif defined(PLATFORM_IS_WINDOWS)
  p->access_time_nsec = 0;
  p->modified_time_nsec = 0;
  p->change_time_nsec = 0;
#endif

  return true;
}

PONY_EXTERN_C_END
