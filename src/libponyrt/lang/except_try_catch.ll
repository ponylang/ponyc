; This file implements the `pony_try` function in LLVM IR as it cannot be
; implemented in C.
; Since the LLVM project doesn't particularly care about IR backwards
; compatibility, please try to keep this file as minimal as possible in order
; to minimise the chances of breakage with newer LLVM versions.

declare i32 @ponyint_personality_v0(...)

define i1 @pony_try(void (i8*)* %fun, i8* %ctx) personality i32 (...)* @ponyint_personality_v0 {
entry:
  invoke void %fun(i8* %ctx)
    to label %post unwind label %unwind

unwind:
  %lp = landingpad { i8*, i32 }
    catch i8* null
  br label %post

post:
  %ret = phi i1 [ true, %entry ], [ false, %unwind ]
  ret i1 %ret
}
