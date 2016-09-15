"""
# Builtin package

The builtin package is home to standard library members that require compiler
support. For details on specific packages, see their individual entity entries.
"""
primitive Platform
  fun dragonfly(): Bool => compile_intrinsic
  fun freebsd(): Bool => compile_intrinsic
  fun linux(): Bool => compile_intrinsic
  fun osx(): Bool => compile_intrinsic
  fun posix(): Bool => freebsd() or dragonfly() or linux() or osx()
  fun windows(): Bool => compile_intrinsic

  fun x86(): Bool => compile_intrinsic
  fun arm(): Bool => compile_intrinsic

  fun lp64(): Bool => compile_intrinsic
  fun llp64(): Bool => compile_intrinsic
  fun ilp32(): Bool => compile_intrinsic

  fun native128(): Bool => compile_intrinsic
  fun debug(): Bool => compile_intrinsic
