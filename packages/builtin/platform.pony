primitive Platform
  fun freebsd(): Bool => compiler_intrinsic
  fun linux(): Bool => compiler_intrinsic
  fun osx(): Bool => compiler_intrinsic
  fun posix(): Bool => linux() or osx()
  fun windows(): Bool => compiler_intrinsic

  fun has_i128(): Bool => compiler_intrinsic
  fun debug(): Bool => compiler_intrinsic
