/**
 * Used to undo any changes llvm_config_begin.h has done
 */
#ifndef LLVM_CONFIG_END_H
#define LLVM_CONFIG_END_H

#if PONY_LLVM < 400
#  pragma pop_macro("NDEBUG")
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#endif
