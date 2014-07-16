#include "../libponyc/pkg/package.h"
#include "../libponyc/ds/stringtab.h"
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

//#if defined(__APPLE__)
#include <getopt.h>
//#endif

static struct option opts[] =
{
  {"ast", no_argument, NULL, 'a'},
  {"path", required_argument, NULL, 'p'},
  {"parse", no_argument, NULL, 'r'},
  {"width", required_argument, NULL, 'w'},
  {NULL, 0, NULL, 0},
};

void usage()
{
  printf(
    "ponyc [OPTIONS] <file>\n"
    "  --ast, -a       print the AST\n"
    "  --path, -p      add additional colon separated search paths\n"
    "  --parse, -r     stop after parse phase\n"
    "  --width, -w     width to target when printing the AST\n"
    );
}

size_t get_width()
{
  struct winsize ws;
  size_t width = 80;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
  {
    if(ws.ws_col > width)
      width = ws.ws_col;
  }

  return width;
}

int main(int argc, char** argv)
{
  package_init(argv[0]);

  bool ast = false;
  bool parse_only = false;
  size_t width = get_width();
  char c;

  while((c = getopt_long(argc, argv, "ap:w:", opts, NULL)) != -1)
  {
    switch(c)
    {
      case 'a': ast = true; break;
      case 'p': package_paths(optarg); break;
      case 'r': parse_only = true; break;
      case 'w': width = atoi(optarg); break;
      default: usage(); return -1;
    }
  }

  argc -= optind;
  argv += optind;

  ast_t* program = program_load((argc > 0) ? argv[0] : ".", parse_only);

  if(ast)
    ast_print(program, width);

  // TODO: detect imported but unused packages in a module
  // TODO: detect unused fields, methods, params, locals
  // TODO: code generation

  ast_free(program);

  print_errors();
  free_errors();

  package_done();
  stringtab_done();

  return (program != NULL) ? 0 : -1;
}
