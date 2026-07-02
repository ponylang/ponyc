// The Windows stable entry point. Installed as <prefix>\bin\<tool>.exe (ponyc,
// pony-lsp, pony-lint, pony-doc), it launches the real, versioned tool binary
// with the same arguments.
//
// Why a shim rather than a symlink (as on Unix): ponyc finds its packages and
// libs relative to its own path, which on Windows comes from GetModuleFileName.
// GetModuleFileName returns the *launch* path, and Windows does not resolve a
// symlink the way Linux /proc/self/exe or macOS realpath do -- so through a
// symlink ponyc would see <prefix>\bin (which has no packages\) and fail. Running
// the real versioned binary through this shim makes GetModuleFileName return the
// versioned path, so resolution works. (Creating a symlink on Windows also needs
// admin or Developer Mode; a shim needs neither.)
//
// How it finds its target without embedding a version that changes each release:
// it reads its own path (so it knows both <prefix>\bin and its own name, e.g.
// ponyc.exe), reads the active version from a pointer file next to it
// (<prefix>\bin\pony-active-version, written by `cmake --install`), and launches
// <prefix>\lib\pony\<version>\bin\<its-own-name>. Rewriting the pointer file
// switches the active version -- the Windows equivalent of repointing the Unix
// symlink.
//
// NOTE: the Ctrl-C / termination handling below is a best effort that was written
// without a Windows machine to test on. It is expected to be verified (and fixed
// if wrong) on Windows before this is relied on.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// Ignore Ctrl-C in the shim itself; the child shares the console and receives the
// same event, so it stops while the shim stays up to forward the child's exit
// code.
static BOOL WINAPI ctrl_handler(DWORD type)
{
  (void)type;
  return TRUE;
}

static int fail(const wchar_t* msg)
{
  fwprintf(stderr, L"ponyc shim: %ls (error %lu)\n", msg, GetLastError());
  return 125;
}

int wmain(void)
{
  // Our own full path, split into directory (<prefix>\bin) and file name.
  wchar_t self[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
  if(n == 0 || n >= MAX_PATH)
    return fail(L"cannot determine own path");

  wchar_t* last = wcsrchr(self, L'\\');
  if(last == NULL)
    return fail(L"own path has no separator");
  *last = L'\0';
  const wchar_t* bindir = self;
  const wchar_t* name = last + 1;

  // <prefix> is the parent of <prefix>\bin.
  wchar_t prefix[MAX_PATH];
  wcsncpy(prefix, bindir, MAX_PATH);
  prefix[MAX_PATH - 1] = L'\0';
  wchar_t* p = wcsrchr(prefix, L'\\');
  if(p == NULL)
    return fail(L"layout unexpected: no prefix above bin");
  *p = L'\0';

  // Active version from the pointer file next to us.
  wchar_t pointer[MAX_PATH];
  _snwprintf(pointer, MAX_PATH, L"%ls\\pony-active-version", bindir);
  FILE* f = _wfopen(pointer, L"r");
  if(f == NULL)
    return fail(L"cannot open pony-active-version");
  wchar_t version[256];
  if(fgetws(version, 256, f) == NULL)
  {
    fclose(f);
    return fail(L"pony-active-version is empty");
  }
  fclose(f);
  size_t len = wcslen(version);
  while(len > 0 && (version[len - 1] == L'\n' || version[len - 1] == L'\r' ||
                    version[len - 1] == L' ' || version[len - 1] == L'\t'))
    version[--len] = L'\0';
  if(len == 0)
    return fail(L"pony-active-version has no version");

  // Target: <prefix>\lib\pony\<version>\bin\<name>.
  wchar_t target[MAX_PATH];
  int t = _snwprintf(target, MAX_PATH, L"%ls\\lib\\pony\\%ls\\bin\\%ls",
    prefix, version, name);
  if(t < 0 || t >= MAX_PATH)
    return fail(L"target path too long");

  // Command line: "target" followed by our original args, taken from the raw
  // command line after our own program name so the original quoting is kept.
  wchar_t* rest = GetCommandLineW();
  if(*rest == L'"')
  {
    rest++;
    while(*rest && *rest != L'"')
      rest++;
    if(*rest == L'"')
      rest++;
  }
  else
  {
    while(*rest && *rest != L' ' && *rest != L'\t')
      rest++;
  }
  while(*rest == L' ' || *rest == L'\t')
    rest++;

  size_t cap = wcslen(target) + wcslen(rest) + 8;
  wchar_t* cmd = (wchar_t*)malloc(cap * sizeof(wchar_t));
  if(cmd == NULL)
    return fail(L"out of memory");
  _snwprintf(cmd, cap, L"\"%ls\" %ls", target, rest);

  // A job object so a killed shim takes the child with it (no orphaned compiler).
  // Ctrl-C reaches the child through the shared console; the shim ignores it (see
  // ctrl_handler) and waits for the child to exit.
  SetConsoleCtrlHandler(ctrl_handler, TRUE);
  HANDLE job = CreateJobObjectW(NULL, NULL);
  if(job != NULL)
  {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji;
    ZeroMemory(&ji, sizeof(ji));
    ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji,
      sizeof(ji));
  }

  STARTUPINFOW si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  if(!CreateProcessW(target, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
  {
    free(cmd);
    return fail(L"cannot launch versioned binary");
  }
  if(job != NULL)
    AssignProcessToJobObject(job, pi.hProcess);

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 125;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  free(cmd);
  return (int)code;
}
