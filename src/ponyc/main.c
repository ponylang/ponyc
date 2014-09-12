#include "../libponyc/ast/parserapi.h"
#include "../libponyc/pkg/package.h"
#include "../libponyc/pass/pass.h"
#include "../libponyc/ds/stringtab.h"
#include "../libponyc/platform/platform.h"

#include "options.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

enum
{
  OPT_OPTIMISE,
  OPT_PATHS,
  OPT_CPU,
  OPT_FEATURES,

  OPT_PASSES,
  OPT_AST,
  OPT_TRACE,
  OPT_WIDTH
};

static arg_t args[] =
{
  {"opt", 'o', ARGUMENT_NONE, OPT_OPTIMISE},
  {"path", 'p', ARGUMENT_REQUIRED, OPT_PATHS},
  {"cpu", 'c', ARGUMENT_REQUIRED, OPT_CPU},
  {"features", 'f', ARGUMENT_REQUIRED, OPT_FEATURES},

  {"pass", 'r', ARGUMENT_REQUIRED, OPT_PASSES},
  {"ast", 'a', ARGUMENT_NONE, OPT_AST},
  {"trace", 't', ARGUMENT_NONE, OPT_TRACE},
  {"width", 'w', ARGUMENT_REQUIRED, OPT_WIDTH},
  ARGUMENTS_FINISH
};

void usage()
{
  printf(
    "ponyc [OPTIONS] <package directory>\n"
    "\n"
    "Often needed options:\n"
    "  --opt, -o       Optimise the output.\n"
    "  --path, -p      Add additional colon separated search paths.\n"
    "    =path[:path]  Used to find packages and libraries.\n"
    "\n"
    "Rarely needed options:\n"
    "  --cpu, -c       Set the target CPU.\n"
    "    =name         Default is the host CPU.\n"
    "  --features, -f  CPU features to enable or disable.\n"
    "    =+this,-that  Use + to enable, - to disable.\n"
    "\n"
    "Debugging options:\n"
    "  --pass, -r      Restrict phases.\n"
    "    =parse\n"
    "    =parsefix\n"
    "    =sugar\n"
    "    =scope1\n"
    "    =name\n"
    "    =flatten\n"
    "    =traits\n"
    "    =scope2\n"
    "    =expr\n"
    "    =ir           Output LLVM IR.\n"
    "    =bitcode      Output LLVM bitcode.\n"
    "    =asm          Output assembly.\n"
    "    =obj          Output an object file.\n"
    "    =all          The default: generate an executable.\n"
    "  --ast, -a       Output an abstract syntax tree.\n"
    "  --trace, -t     Enable parse trace.\n"
    "  --width, -w     Width to target when printing the AST.\n"
    "    =columns      Defaults to the terminal width.\n"
    "\n"
    );
}

size_t get_width()
{
  size_t width = 80;
#ifdef _WIN64
  CONSOLE_SCREEN_BUFFER_INFO info;

  if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
  {
    int cols = info.srWindow.Right - info.srWindow.Left + 1;

    if(cols > width)
      width = cols;
  }
#else
  struct winsize ws;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
  {
    if(ws.ws_col > width)
      width = ws.ws_col;
  }
#endif
  return width;
}

int main(int argc, char* argv[])
{
  package_init(argv[0]);

  pass_opt_t opt;
  memset(&opt, 0, sizeof(pass_opt_t));

  size_t width = get_width();
  bool print_ast = false;

  parse_state_t s;
  opt_init(args, &s, &argc, argv);

  int id;
  while((id = opt_next(&s)) != -1)
  {
    switch(id)
    {
      case OPT_OPTIMISE: opt.opt = true; break;
      case OPT_PATHS: package_add_paths(s.arg_val); break;
      case OPT_CPU: opt.cpu = s.arg_val; break;
      case OPT_FEATURES: opt.features = s.arg_val; break;

      case OPT_AST: print_ast = true; break;
      case OPT_TRACE: parse_trace(true); break;
      case OPT_WIDTH: width = atoi(s.arg_val); break;

      case OPT_PASSES:
        if(!limit_passes(s.arg_val))
        {
          usage();
          return -1;
        }
        break;

      default: usage(); return -1;
    }
  }

  ast_setwidth(width);
  const char* path;

  switch(argc)
  {
    case 1: path = "."; break;
    case 2: path = argv[1]; break;
    default: usage(); return -1;
  }

  if(path[0] == '-')
  {
    usage();
    return -1;
  }

  ast_t* program = program_load(path);
  int ret = 0;

  if(program != NULL)
  {
    if(print_ast)
      ast_print(program);

    if(!program_passes(program, &opt))
      ret = -1;

    ast_free(program);
  } else {
    ret = -1;
  }

  print_errors();
  free_errors();

  package_done();
  stringtab_done();

  return ret;
}
