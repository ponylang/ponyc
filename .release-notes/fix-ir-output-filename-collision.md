## Fix `ponyc -r=ir` writing builtin's IR over the program's IR

When emitting LLVM IR with `-r=ir`, ponyc wrote both the program's module and builtin's module to the same `.main.ll` file. Builtin was written second, so the file contained only builtin's definitions — the program's IR was lost. Each module is now written to a distinct file: `<name>.main.ll` for the program and `<name>.builtin.ll` for builtin.
