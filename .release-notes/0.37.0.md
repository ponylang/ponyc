## Fix unsound return types being allowed for autorecover

The existing autorecover code was checking whether it was safe to write to the recovery cap, regardless of whether it was an argument. That's correct for arguments, but for result types we need to ensure that it's safe to extract.

Safety to extract is derived from Steed's extracting table: a type is safe to extract if results in itself under extracting viewpoint adaptation. Thus it is unsound to extract trn from trn, but it is sound to extract box.

## Revert RFC 67

The original intent behind RFC0067 was to make it easy to add a prefix with the
log level for any given log message. However, there is a logic bug in the
implementation; the log level prefix added to messages in LogFormatter would
always be the log level specified in Logger.create(), not the log level desired
at specific call sites. This would lead to misleading log messages where the log
level prefix doesn't match the severity of the log message, such as "FINE:
Critical error. Shutting down".

It would be better if these changes were reverted so a more thought-out solution
could be implemented in the future.

## Always allow field access for objects defined inside a recover block

Some field accesses were incorrectly being rejected inside recover blocks. This fix allows all nested field accesses on variables which were defined inside the recover block, like the examples below:

```pony
recover
   let x = ...
   let y = x.y
   x.y = Foo
   x.y.z = y.z
end
```
