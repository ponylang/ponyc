primitive Platform
  fun freebsd(): Bool => compile_intrinsic
  fun linux(): Bool => compile_intrinsic
  fun osx(): Bool => compile_intrinsic
  fun posix(): Bool => freebsd() or linux() or osx()
  fun windows(): Bool => compile_intrinsic

  fun lp64(): Bool => compile_intrinsic
  fun llp64(): Bool => compile_intrinsic
  fun ilp32(): Bool => compile_intrinsic

  fun has_i128(): Bool => compile_intrinsic
  fun debug(): Bool => compile_intrinsic
