/**
 * Include this before including any llvm headers
 * And then include llvm_config_end.h after those
 * headers.
 */

#ifndef LLVM_CONFIG_BEGIN_H
#define LLVM_CONFIG_BEGIN_H

#ifdef _MSC_VER
#  pragma warning(push)

//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#  pragma warning(disable:4624)
#  pragma warning(disable:4141)
#  pragma warning(disable:4291)
#  pragma warning(disable:4146)

// LLVM claims DEBUG as a macro name. Conflicts with MSVC headers.
#  pragma warning(disable:4005)
#endif

#endif
