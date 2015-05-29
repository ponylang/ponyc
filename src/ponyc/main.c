#include "../libponyc/ast/parserapi.h"
#include "../libponyc/ast/bnfprint.h"
#include "../libponyc/pkg/package.h"
#include "../libponyc/pass/pass.h"
#include "../libponyc/ast/stringtab.h"
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
  OPT_VERSION,
  OPT_DEBUG,
  OPT_STRIP,
  OPT_PATHS,
  OPT_OUTPUT,
  OPT_LIBRARY,

  OPT_SAFE,
  OPT_IEEEMATH,
  OPT_RESTRICT,
  OPT_CPU,
  OPT_FEATURES,
  OPT_TRIPLE,
  OPT_STATS,
  OPT_PRINT_PATHS,

  OPT_PASSES,
  OPT_AST,
  OPT_TRACE,
  OPT_WIDTH,
  OPT_IMMERR,
  OPT_VERIFY,

  OPT_BNF,
  OPT_ANTLR,
  OPT_ANTLRRAW
};

static opt_arg_t args[] =
{
  {"version", 'v', OPT_ARG_NONE, OPT_VERSION},
  {"debug", 'd', OPT_ARG_NONE, OPT_DEBUG},
  {"strip", 's', OPT_ARG_NONE, OPT_STRIP},
  {"path", 'p', OPT_ARG_REQUIRED, OPT_PATHS},
  {"output", 'o', OPT_ARG_REQUIRED, OPT_OUTPUT},
  {"library", 'l', OPT_ARG_NONE, OPT_LIBRARY},

  {"safe", 0, OPT_ARG_OPTIONAL, OPT_SAFE},
  {"ieee-math", 0, OPT_ARG_NONE, OPT_IEEEMATH},
  {"restrict", 0, OPT_ARG_NONE, OPT_RESTRICT},
  {"cpu", 0, OPT_ARG_REQUIRED, OPT_CPU},
  {"features", 0, OPT_ARG_REQUIRED, OPT_FEATURES},
  {"triple", 0, OPT_ARG_REQUIRED, OPT_TRIPLE},
  {"stats", 0, OPT_ARG_NONE, OPT_STATS},
  {"print-paths", 0, OPT_ARG_NONE, OPT_PRINT_PATHS},

  {"pass", 'r', OPT_ARG_REQUIRED, OPT_PASSES},
  {"ast", 'a', OPT_ARG_NONE, OPT_AST},
  {"trace", 't', OPT_ARG_NONE, OPT_TRACE},
  {"width", 'w', OPT_ARG_REQUIRED, OPT_WIDTH},
  {"immerr", '\0', OPT_ARG_NONE, OPT_IMMERR},
  {"verify", '\0', OPT_ARG_NONE, OPT_VERIFY},

  {"bnf", '\0', OPT_ARG_NONE, OPT_BNF},
  {"antlr", '\0', OPT_ARG_NONE, OPT_ANTLR},
  {"antlrraw", '\0', OPT_ARG_NONE, OPT_ANTLRRAW},

  OPT_ARGS_FINISH
};

static void usage()
{
  printf(
    "ponyc [OPTIONS] <package directory>\n"
    "\n"
    "The package directory defaults to the current directory.\n"
    "\n"
    "Options:\n"
    "  --version, -v   Print the version of the compiler and exit.\n"
    "  --debug, -d     Don't optimise the output.\n"
    "  --strip, -s     Strip debug info.\n"
    "  --path, -p      Add an additional search path.\n"
    "    =path         Used to find packages and libraries.\n"
    "  --output, -o    Write output to this directory.\n"
    "    =path         Defaults to the current directory.\n"
    "  --library, -l   Generate a C-API compatible static library.\n"
    "\n"
    "Rarely needed options:\n"
    "  --safe          Allow only the listed packages to use C FFI.\n"
    "    =this,that    With no packages listed, only builtin is allowed.\n"
    "  --ieee-math     Force strict IEEE 754 compliance.\n"
    "  --restrict      FORTRAN pointer semantics.\n"
    "  --cpu           Set the target CPU.\n"
    "    =name         Default is the host CPU.\n"
    "  --features      CPU features to enable or disable.\n"
    "    =+this,-that  Use + to enable, - to disable.\n"
    "  --triple        Set the target triple.\n"
    "    =name         Defaults to the host triple.\n"
    "  --stats         Print some compiler stats.\n"
    "  --print-paths   Print search paths.\n"
    "\n"
    "Debugging options:\n"
    "  --pass, -r      Restrict phases.\n"
    "    =parse\n"
    "    =syntax\n"
    "    =sugar\n"
    "    =scope\n"
    "    =name\n"
    "    =flatten\n"
    "    =traits\n"
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
    "  --immerr        Report errors immediately rather than deferring.\n"
    "  --verify        Verify LLVM IR.\n"
    "  --bnf           Print out the Pony grammar as human readable BNF.\n"
    "  --antlr         Print out the Pony grammar as an ANTLR file.\n"
    "\n"
    "Runtime options for Pony programs (not for use with ponyc):\n"
    "  --ponythreads   Use N scheduler threads. Defaults to the number of\n"
    "                  cores (not hyperthreads) available.\n"
    "  --ponycdmin     Defer cycle detection until 2^N actors have blocked.\n"
    "                  Defaults to 2^4.\n"
    "  --ponycdmax     Always cycle detect when 2^N actors have blocked.\n"
    "                  Defaults to 2^18.\n"
    "  --ponycdconf    Send cycle detection CNF messages in groups of 2^N.\n"
    "                  Defaults to 2^6.\n"
    "  --ponygcinitial Defer garbage collection until an actor is using at\n"
    "                  least 2^N bytes. Defaults to 2^14.\n"
    "  --ponygcfactor  After GC, an actor will next be GC'd at a heap memory\n"
    "                  usage N times its current value. This is a floating\n"
    "                  point value. Defaults to 2.0.\n"
    "  --ponysched     Use an alternate scheduling algorithm.\n"
    "    =mpmcq        The default scheduler.\n"
    "    =coop         An experimental cooperative scheduler.\n"
    "  --ponynoyield   Do not yield the CPU when no work is available.\n"
    );
}

static size_t get_width()
{
  size_t width = 80;
#ifdef PLATFORM_IS_WINDOWS
  if(_isatty(_fileno(stdout)))
  {
    CONSOLE_SCREEN_BUFFER_INFO info;

    if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
    {
      int cols = info.srWindow.Right - info.srWindow.Left + 1;

      if(cols > width)
        width = cols;
    }
  }
#else
  if(isatty(STDOUT_FILENO))
  {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
    {
      if(ws.ws_col > width)
        width = ws.ws_col;
    }
  }
#endif
  return width;
}

static bool compile_package(const char* path, pass_opt_t* opt, bool print_ast)
{
  ast_t* program = program_load(path, opt);

  if(program == NULL)
    return false;

  if(print_ast)
    ast_print(program);

  bool ok = generate_passes(program, opt);
  ast_free(program);
  return ok;
}

int main(int argc, char* argv[])
{
  stringtab_init();

  pass_opt_t opt;
  pass_opt_init(&opt);

  opt.release = true;
  opt.no_restrict = true;
  opt.output = ".";

  ast_setwidth(get_width());
  bool print_ast = false;

  opt_state_t s;
  opt_init(args, &s, &argc, argv);

  bool ok = true;
  bool print_usage = false;
  bool print_paths = false;
  int id;

  while((id = opt_next(&s)) != -1)
  {
    switch(id)
    {
      case OPT_VERSION:
        printf("%s\n", PONY_VERSION);
        return 0;

      case OPT_DEBUG: opt.release = false; break;
      case OPT_STRIP: opt.strip_debug = true; break;
      case OPT_PATHS: package_add_paths(s.arg_val); break;
      case OPT_OUTPUT: opt.output = s.arg_val; break;
      case OPT_LIBRARY: opt.library = true; break;

      case OPT_SAFE:
        if(!package_add_safe(s.arg_val))
          ok = false;
        break;

      case OPT_IEEEMATH: opt.ieee_math = true; break;
      case OPT_RESTRICT: opt.no_restrict = false; break;
      case OPT_CPU: opt.cpu = s.arg_val; break;
      case OPT_FEATURES: opt.features = s.arg_val; break;
      case OPT_TRIPLE: opt.triple = s.arg_val; break;
      case OPT_STATS: opt.print_stats = true; break;
      case OPT_PRINT_PATHS: print_paths = true; break;

      case OPT_AST: print_ast = true; break;
      case OPT_TRACE: parse_trace(true); break;
      case OPT_WIDTH: ast_setwidth(atoi(s.arg_val)); break;
      case OPT_IMMERR: error_set_immediate(true); break;
      case OPT_VERIFY: opt.verify = true; break;

      case OPT_BNF: print_grammar(false, true); return 0;
      case OPT_ANTLR: print_grammar(true, true); return 0;
      case OPT_ANTLRRAW: print_grammar(true, false); return 0;

      case OPT_PASSES:
        if(!limit_passes(&opt, s.arg_val))
        {
          ok = false;
          print_usage = true;
        }
        break;

      default: usage(); return -1;
    }
  }

  for(int i = 1; i < argc; i++)
  {
    if(argv[i][0] == '-')
    {
      fprintf(stderr, "Unrecognised option: %s\n", argv[i]);
      ok = false;
      print_usage = true;
    }
  }

#ifdef PLATFORM_IS_WINDOWS
  opt.strip_debug = true;
#endif

  if(!ok)
  {
    print_errors();

    if(print_usage)
      usage();

    return -1;
  }

  if(package_init(&opt))
  {
    if(print_paths)
    {
      for(strlist_t* p = package_paths(); p; p = strlist_next(p))
        printf("%s\n", strlist_data(p));
      return 0;
    }
    if(argc == 1)
    {
      ok &= compile_package(".", &opt, print_ast);
    } else {
      for(int i = 1; i < argc; i++)
        ok &= compile_package(argv[i], &opt, print_ast);
    }
  }

  if(!ok && get_error_count() == 0)
    printf("Error: internal failure not reported\n");

  package_done(&opt);
  pass_opt_done(&opt);
  stringtab_done();

  return ok ? 0 : -1;
}
