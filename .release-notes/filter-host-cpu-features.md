## Fix warnings about unrecognized CPU features on native builds

Building ponyc natively on machines whose CPUs advertise deprecated extensions (such as Intel Xeon Phi's `avx512pf` and `prefetchwt1`) produced warnings like `'-avx512pf' is not a recognized feature for this target (ignoring feature)`. The auto-detected host features are now filtered to those the compiler's LLVM backend recognizes before they reach code generation.
