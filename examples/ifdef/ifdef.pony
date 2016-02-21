// The types for printf are actually the same in Windows and other OSes, but
// for the purposes of an example that everyone can compile without an external
// library we'll assume they're different.

// Give FFI declaration with a guard for each platform.
use @printf[I32](fmt: Pointer[U8] tag, ...) if windows
use @printf[I32](fmt: Pointer[U8] tag, ...) if not windows

actor Main
  new create(env: Env) =>
    // This would be an error, since we don't know which declaration to use:
    //@printf("Hello\n".cstring())

    ifdef windows then
      // We only get here in Windows, so we know which FFI declaration to use.
      @printf("Hello Windows\n".cstring())
    elseif linux then
      // Again we know which FFI declaration to use here.
      @printf("Hello Linux\n".cstring())
    else
      // And again we know which FFI declaration to use here.
      @printf("Hello everyone else\n".cstring())
    end

    // Define build flag "foo" on the ponyc command line to change this value:
    //   -D=foo
    let x: U64 = 
      ifdef "foo" then
        3
      else
        4
      end

    env.out.print(x.string())

    // Define build flag "fail" to hit this compile error:
    //   -D=fail
    ifdef "fail" then
      compile_error "We don't support failing"
    else
      env.out.print("Not failing")
    end
