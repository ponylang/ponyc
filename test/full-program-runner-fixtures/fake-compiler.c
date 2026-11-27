// A stand-in for ponyc used by cmake/RunExpectationChecks.cmake to test the
// full-program runner's handling of compiler output. The runner starts the
// compiler with the test directory as its working directory.
//
// If the directory holds fake-stderr.txt, its bytes are written to stderr
// unchanged and the program exits the way ponyc does when compilation fails
// (main returns -1). Otherwise it stands in for a compiler crash: abort() on
// POSIX; on Windows it exits with 3, because abort() in an MSVC debug build
// can open a dialog and hang the test.
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(void)
{
  FILE* f = fopen("fake-stderr.txt", "rb");

  if(f == NULL)
  {
#ifdef _WIN32
    return 3;
#else
    abort();
#endif
  }

#ifdef _WIN32
  _setmode(_fileno(stderr), _O_BINARY);
#endif

  char buf[4096];
  size_t n;

  while((n = fread(buf, 1, sizeof(buf), f)) > 0)
    fwrite(buf, 1, n, stderr);

  fclose(f);
  return -1;
}
