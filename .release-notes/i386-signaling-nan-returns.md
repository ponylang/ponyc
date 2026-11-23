## Preserve signaling NaNs in Pony float returns on 32-bit x86

On 32-bit x86, ordinary Pony methods preserve the bit pattern of signaling NaNs when returning an `F32` or `F64`. Calls through the C ABI, including bare methods and bare lambdas, can still set the quiet bit.
