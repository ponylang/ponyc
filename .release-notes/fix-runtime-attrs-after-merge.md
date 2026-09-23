## Fix optimizer attributes lost after runtime linking

After the runtime was linked into the program, LLVM attributes on runtime functions were lost. Without them, unnecessary unwind paths remained at every runtime call site and memory operations could not be moved past allocator calls. The attributes are now preserved across linking.
