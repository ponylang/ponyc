primitive Platform
  fun freebsd(): Bool => compiler_intrinsic
  fun linux(): Bool => compiler_intrinsic
  fun osx(): Bool => compiler_intrinsic
  fun posix(): Bool => freebsd() or linux() or osx()
  fun windows(): Bool => compiler_intrinsic

  fun lp64(): Bool => compiler_intrinsic
  fun llp64(): Bool => compiler_intrinsic
  fun ilp32(): Bool => compiler_intrinsic

  fun has_i128(): Bool => compiler_intrinsic
  fun debug(): Bool => compiler_intrinsic
