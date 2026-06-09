## Use embedded LLD for native macOS sanitizer builds

Native macOS sanitizer builds (`use=address_sanitizer`, `use=undefined_behavior_sanitizer`) now link through the embedded ld64.lld linker instead of requiring an external linker.
