"""
Before issue #4475 was fixed, LLVM module checking failed in the very specific
case of generating the Pony statement error before generating a termination
instruction (e.g. ret).

Here's an example of code generation that was problematic:

define private fastcc void @Foo_ref_create_Io(ptr %this, i32 %a) unnamed_addr !dbg !882 !pony.abi !4 {
entry:
  %this1 = alloca ptr, align 8
  store ptr %this, ptr %this1, align 8
  ; error
  ;;;;;;;;;;;;
  call void @llvm.dbg.declare(metadata ptr %this1, metadata !885, metadata !DIExpression()), !dbg !887
  %a2 = alloca i32, align 4
  store i32 %a, ptr %a2, align 4
  call void @llvm.dbg.declare(metadata ptr %a2, metadata !888, metadata !DIExpression()), !dbg !889
  call void @pony_error(), !dbg !890
  unreachable, !dbg !890
  ;;;;;;;;;;;;
  ret void, !dbg !890 ; => problematic instruction, because the last instruction is terminator 
}
"""
class Foo
  new create(a: U32) ? =>
    error

actor Main
  new create(env: Env) =>
    try
      let f = Foo(1)?
    end
