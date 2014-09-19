#include "../libponyc/ast/parserapi.h"
#include "../libponyc/pkg/package.h"
#include "../libponyc/pass/pass.h"
#include "../libponyc/ds/stringtab.h"
#include <platform.h>

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
  OPT_OUTPUT,

  OPT_CPU,
  OPT_FEATURES,
  OPT_TRIPLE,

  OPT_PASSES,
  OPT_AST,
  OPT_TRACE,
  OPT_WIDTH,
  OPT_IMMERR
};

static opt_arg_t args[] =
{
  {"opt", 'O', OPT_ARG_NONE, OPT_OPTIMISE},
  {"path", 'p', OPT_ARG_REQUIRED, OPT_PATHS},
  {"output", 'o', OPT_ARG_REQUIRED, OPT_OUTPUT},

  {"cpu", 'c', OPT_ARG_REQUIRED, OPT_CPU},
  {"features", 'f', OPT_ARG_REQUIRED, OPT_FEATURES},
  {"triple", 0, OPT_ARG_REQUIRED, OPT_TRIPLE},

  {"pass", 'r', OPT_ARG_REQUIRED, OPT_PASSES},
  {"ast", 'a', OPT_ARG_NONE, OPT_AST},
  {"trace", 't', OPT_ARG_NONE, OPT_TRACE},
  {"width", 'w', OPT_ARG_REQUIRED, OPT_WIDTH},
  {"immerr", '\0', OPT_ARG_NONE, OPT_IMMERR},
  OPT_ARGS_FINISH
};

static void usage()
{
  printf(
    "ponyc [OPTIONS] <package directory>\n"
    "\n"
    "The package directory defaults to the current directory."
    "\n"
    "Often needed options:\n"
    "  --opt, -O       Optimise the output.\n"
    "  --path, -p      Add an additional search path.\n"
    "    =path         Used to find packages and libraries.\n"
    "  --output, -o    Write output to this directory.\n"
    "    =path         Defaults to the current directory.\n"
    "\n"
    "Rarely needed options:\n"
    "  --cpu, -c       Set the target CPU.\n"
    "    =name         Default is the host CPU.\n"
    "  --features, -f  CPU features to enable or disable.\n"
    "    =+this,-that  Use + to enable, - to disable.\n"
    "  --triple        Set the target triple.\n"
    "    =name         Defaults to the host triple.\n"
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
    "  --immerr        Report errors imemdiately rather than deferring.\n"
    "\n"
    );
}

static size_t get_width()
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

static bool compile_package(const char* path, pass_opt_t* opt, bool print_ast)
{
  ast_t* program = program_load(path);

  if(program == NULL)
  {
    print_errors();
    free_errors();
    return false;
  }

  if(print_ast)
    ast_print(program);

  bool ok = program_passes(program, opt);
  ast_free(program);

  print_errors();
  free_errors();

  return ok;
}

int main(int argc, char* argv[])
{
  pass_opt_t opt;
  memset(&opt, 0, sizeof(pass_opt_t));
  opt.output = ".";

  ast_setwidth(get_width());
  bool print_ast = false;

  opt_state_t s;
  opt_init(args, &s, &argc, argv);

  int id;

  while((id = opt_next(&s)) != -1)
  {
    switch(id)
    {
      case OPT_OPTIMISE: opt.opt = true; break;
      case OPT_PATHS: package_add_paths(s.arg_val); break;
      case OPT_OUTPUT: opt.output = s.arg_val; break;

      case OPT_CPU: opt.cpu = s.arg_val; break;
      case OPT_FEATURES: opt.features = s.arg_val; break;
      case OPT_TRIPLE: opt.triple = s.arg_val; break;

      case OPT_AST: print_ast = true; break;
      case OPT_TRACE: parse_trace(true); break;
      case OPT_WIDTH: ast_setwidth(atoi(s.arg_val)); break;
      case OPT_IMMERR: error_set_immediate(true); break;

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

  bool ok = true;

  for(int i = 1; i < argc; i++)
  {
    if(argv[i][0] == '-')
    {
      printf("Unrecognised option: %s\n", argv[i]);
      ok = false;
    }
  }

  if(!ok)
  {
    printf("\n");
    usage();
    return -1;
  }

  if(package_init(argv[0], &opt))
  {
    if(argc == 1)
    {
      ok &= compile_package(".", &opt, print_ast);
    } else {
      for(int i = 1; i < argc; i++)
        ok &= compile_package(argv[i], &opt, print_ast);
    }

    package_done(&opt);
  }

  stringtab_done();

  return ok ? 0 : -1;
}
