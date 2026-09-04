## Fix lambda parameter type inference regression

Lambda parameters that relied on type inference from the calling context stopped compiling after the addition of generic type argument inference. Code like `m.upsert("key", 1, {(old, cur) => old + cur })` produced "a lambda parameter must specify a type or be inferable from context" where it previously compiled without error. Lambda parameter types are once again inferred from the expected function type at the call site.
