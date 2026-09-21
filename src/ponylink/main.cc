// ponylink — standalone linker for ponyc.
//
// Reads lld arguments from a file (one per line) and calls lld::lldMain().
// ponyc spawns this as a subprocess on ILP32 targets so lld starts in a
// clean address space.

#include <lld/Common/Driver.h>
#include <llvm/Support/raw_ostream.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

LLD_HAS_DRIVER(elf)
LLD_HAS_DRIVER(macho)
LLD_HAS_DRIVER(coff)
LLD_HAS_DRIVER(mingw)
LLD_HAS_DRIVER(wasm)

int main(int argc, char* argv[])
{
  if(argc != 2)
  {
    fprintf(stderr, "usage: %s <args-file>\n", argv[0]);
    return 1;
  }

  std::ifstream f(argv[1]);
  if(!f)
  {
    fprintf(stderr, "ponylink: cannot open %s: %s\n",
      argv[1], strerror(errno));
    return 1;
  }

  std::vector<std::string> arg_strings;
  std::string line;
  while(std::getline(f, line))
  {
    if(!line.empty())
      arg_strings.push_back(std::move(line));
  }

  std::vector<const char*> args;
  for(auto& s : arg_strings)
    args.push_back(s.c_str());

  std::string lld_stderr_str;
  llvm::raw_string_ostream lld_stderr(lld_stderr_str);

  lld::Result result = lld::lldMain(
    args,
    llvm::outs(),
    lld_stderr,
    {{lld::WinLink, &lld::coff::link},
     {lld::Gnu, &lld::elf::link},
     {lld::Darwin, &lld::macho::link},
     {lld::MinGW, &lld::mingw::link},
     {lld::Wasm, &lld::wasm::link}});

  if(!lld_stderr_str.empty())
    fprintf(stderr, "%s", lld_stderr_str.c_str());

  return result.retCode;
}
