## Fix bug where Flags.remove could set flags in addition to unsetting them

Flags.remove when given a flag to remove that wasn't currently present in the set, would turn the flag on.
It should only be turning flags off, not turning them on.

## Allow Flags instances to be created with a set bit encoding

Extending Flags.create to (optionally) allow initialization of a Flags
object with the numeric representation populated. This value defaults
to 0 (no flags set).

