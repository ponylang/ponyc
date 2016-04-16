#include "../libponyc/ponyc.h"
#include "../libponyc/ast/parserapi.h"
#include "../libponyc/ast/bnfprint.h"
#include "../libponyc/pkg/package.h"
#include "../libponyc/pkg/buildflagset.h"
#include "../libponyc/pass/pass.h"
#include "../libponyc/ast/stringtab.h"
#include "../libponyc/ast/treecheck.h"
#include <platform.h>
#include "../libponyrt/options/options.h"

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
  OPT_BUILDFLAG,
  OPT_STRIP,
  OPT_PATHS,
  OPT_OUTPUT,
  OPT_LIBRARY,
  OPT_DOCS,

  OPT_SAFE,
  OPT_IEEEMATH,
  OPT_CPU,
  OPT_FEATURES,
  OPT_TRIPLE,
  OPT_STATS,

  OPT_VERBOSE,
  OPT_PASSES,
  OPT_AST,
  OPT_ASTPACKAGE,
  OPT_TRACE,
  OPT_WIDTH,
  OPT_IMMERR,
  OPT_VERIFY,
  OPT_FILENAMES,
  OPT_CHECKTREE,

  OPT_BNF,
  OPT_ANTLR,
  OPT_ANTLRRAW,
  OPT_PIC
};

static opt_arg_t args[] =
{
  {"version", 'v', OPT_ARG_NONE, OPT_VERSION},
  {"debug", 'd', OPT_ARG_NONE, OPT_DEBUG},
  {"define", 'D', OPT_ARG_REQUIRED, OPT_BUILDFLAG},
  {"strip", 's', OPT_ARG_NONE, OPT_STRIP},
  {"path", 'p', OPT_ARG_REQUIRED, OPT_PATHS},
  {"output", 'o', OPT_ARG_REQUIRED, OPT_OUTPUT},
  {"library", 'l', OPT_ARG_NONE, OPT_LIBRARY},
  {"docs", 'g', OPT_ARG_NONE, OPT_DOCS},

  {"safe", 0, OPT_ARG_OPTIONAL, OPT_SAFE},
  {"ieee-math", 0, OPT_ARG_NONE, OPT_IEEEMATH},
  {"cpu", 0, OPT_ARG_REQUIRED, OPT_CPU},
  {"features", 0, OPT_ARG_REQUIRED, OPT_FEATURES},
  {"triple", 0, OPT_ARG_REQUIRED, OPT_TRIPLE},
  {"stats", 0, OPT_ARG_NONE, OPT_STATS},

  {"verbose", 'V', OPT_ARG_REQUIRED, OPT_VERBOSE},
  {"pass", 'r', OPT_ARG_REQUIRED, OPT_PASSES},
  {"ast", 'a', OPT_ARG_NONE, OPT_AST},
  {"astpackage", 0, OPT_ARG_NONE, OPT_ASTPACKAGE},
  {"trace", 't', OPT_ARG_NONE, OPT_TRACE},
  {"width", 'w', OPT_ARG_REQUIRED, OPT_WIDTH},
  {"immerr", '\0', OPT_ARG_NONE, OPT_IMMERR},
  {"verify", '\0', OPT_ARG_NONE, OPT_VERIFY},
  {"files", '\0', OPT_ARG_NONE, OPT_FILENAMES},
  {"checktree", '\0', OPT_ARG_NONE, OPT_CHECKTREE},

  {"bnf", '\0', OPT_ARG_NONE, OPT_BNF},
  {"antlr", '\0', OPT_ARG_NONE, OPT_ANTLR},
  {"antlrraw", '\0', OPT_ARG_NONE, OPT_ANTLRRAW},
  {"pic", '\0', OPT_ARG_NONE, OPT_PIC},
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
    "  --define, -D    Define the specified build flag.\n"
    "    =name\n"
    "  --strip, -s     Strip debug info.\n"
    "  --path, -p      Add an additional search path.\n"
    "    =path         Used to find packages and libraries.\n"
    "  --output, -o    Write output to this directory.\n"
    "    =path         Defaults to the current directory.\n"
    "  --library, -l   Generate a C-API compatible static library.\n"
    "  --pic           Compile using position independent code.\n"
    "  --docs, -g      Generate code documentation.\n"
    "\n"
    "Rarely needed options:\n"
    "  --safe          Allow only the listed packages to use C FFI.\n"
    "    =package      With no packages listed, only builtin is allowed.\n"
    "  --ieee-math     Force strict IEEE 754 compliance.\n"
    "  --cpu           Set the target CPU.\n"
    "    =name         Default is the host CPU.\n"
    "  --features      CPU features to enable or disable.\n"
    "    =+this,-that  Use + to enable, - to disable.\n"
    "  --triple        Set the target triple.\n"
    "    =name         Defaults to the host triple.\n"
    "  --stats         Print some compiler stats.\n"
    "\n"
    "Debugging options:\n"
    "  --verbose, -V   Verbosity level.\n"
    "    =0            Only print errors.\n"
    "    =1            Print info on compiler stages.\n"
    "    =2            More detailed compilation information.\n"
    "    =3            External tool command lines.\n"
    "    =4            Very low-level detail.\n"
    "  --pass, -r      Restrict phases.\n"
    "    =parse\n"
    "    =syntax\n"
    "    =sugar\n"
    "    =scope\n"
    "    =import\n"
    "    =name\n"
    "    =flatten\n"
    "    =traits\n"
    "    =docs\n"
    "    =expr\n"
    "    =final\n"
    "    =ir           Output LLVM IR.\n"
    "    =bitcode      Output LLVM bitcode.\n"
    "    =asm          Output assembly.\n"
    "    =obj          Output an object file.\n"
    "    =all          The default: generate an executable.\n"
    "  --ast, -a       Output an abstract syntax tree for the whole program.\n"
    "  --astpackage    Output an abstract syntax tree for the main package.\n"
    "  --trace, -t     Enable parse trace.\n"
    "  --width, -w     Width to target when printing the AST.\n"
    "    =columns      Defaults to the terminal width.\n"
    "  --immerr        Report errors immediately rather than deferring.\n"
    "  --verify        Verify LLVM IR.\n"
    "  --files         Print source file names as each is processed.\n"
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

static bool compile_package(const char* path, pass_opt_t* opt,
  bool print_program_ast, bool print_package_ast)
{
  ast_t* program = program_load(path, opt);

  if(program == NULL)
    return false;

  if(print_program_ast)
    ast_print(program);

  if(print_package_ast)
    ast_print(ast_child(program));

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
  opt.output = ".";

  ast_setwidth(get_width());
  bool print_program_ast = false;
  bool print_package_ast = false;

  opt_state_t s;
  ponyint_opt_init(args, &s, &argc, argv);

  bool ok = true;
  bool print_usage = false;
  int id;

  while((id = ponyint_opt_next(&s)) != -1)
  {
    switch(id)
    {
      case OPT_VERSION:
        printf("%s\n", PONY_VERSION);
        return 0;

      case OPT_DEBUG: opt.release = false; break;
      case OPT_BUILDFLAG: define_build_flag(s.arg_val); break;
      case OPT_STRIP: opt.strip_debug = true; break;
      case OPT_PATHS: package_add_paths(s.arg_val, &opt); break;
      case OPT_OUTPUT: opt.output = s.arg_val; break;
      case OPT_PIC: opt.pic = true; break;
      case OPT_LIBRARY: opt.library = true; break;
      case OPT_DOCS: opt.docs = true; break;

      case OPT_SAFE:
        if(!package_add_safe(s.arg_val, &opt))
          ok = false;
        break;

      case OPT_IEEEMATH: opt.ieee_math = true; break;
      case OPT_CPU: opt.cpu = s.arg_val; break;
      case OPT_FEATURES: opt.features = s.arg_val; break;
      case OPT_TRIPLE: opt.triple = s.arg_val; break;
      case OPT_STATS: opt.print_stats = true; break;

      case OPT_AST: print_program_ast = true; break;
      case OPT_ASTPACKAGE: print_package_ast = true; break;
      case OPT_TRACE: parse_trace(true); break;
      case OPT_WIDTH: ast_setwidth(atoi(s.arg_val)); break;
      case OPT_IMMERR: errors_set_immediate(opt.check.errors, true); break;
      case OPT_VERIFY: opt.verify = true; break;
      case OPT_FILENAMES: opt.print_filenames = true; break;
      case OPT_CHECKTREE: opt.check_tree = true; break;

      case OPT_BNF: print_grammar(false, true); return 0;
      case OPT_ANTLR: print_grammar(true, true); return 0;
      case OPT_ANTLRRAW: print_grammar(true, false); return 0;

      case OPT_VERBOSE:
        {
          int v = atoi(s.arg_val);
          if (v >= 0 && v <= 4) {
            opt.verbosity = (verbosity_level)v;
          } else {
            ok = false;
          }
        }
        break;

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
      printf("Unrecognised option: %s\n", argv[i]);
      ok = false;
      print_usage = true;
    }
  }

#if defined(PLATFORM_IS_WINDOWS)
  opt.strip_debug = true;
#endif

  if(!ok)
  {
    errors_print(opt.check.errors);

    if(print_usage)
      usage();

    return -1;
  }

  if(ponyc_init(&opt))
  {
    if(argc == 1)
    {
      ok &= compile_package(".", &opt, print_program_ast, print_package_ast);
    } else {
      for(int i = 1; i < argc; i++)
        ok &= compile_package(argv[i], &opt, print_program_ast,
          print_package_ast);
    }
  }

  if(!ok && errors_get_count(opt.check.errors) == 0)
    printf("Error: internal failure not reported\n");

  ponyc_shutdown(&opt);
  pass_opt_done(&opt);

  return ok ? 0 : -1;
}
