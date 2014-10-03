primitive Platform
  fun tag linux(): Bool => compiler_intrinsic
  fun tag osx(): Bool => compiler_intrinsic
  fun tag windows(): Bool => compiler_intrinsic

  fun tag has_i128(): Bool => compiler_intrinsic
  fun tag debug(): Bool => compiler_intrinsic
