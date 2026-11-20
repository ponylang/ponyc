SECTIONS {
  .data.rel.ro : ALIGN(8) {
    *(.data.rel.ro .data.rel.ro.*)
    . = ALIGN(8);
    HIDDEN(pony_maps_begin = .);
    KEEP(*(.llvm_stackmaps))
    HIDDEN(pony_maps_end = .);
  }
} INSERT BEFORE .dynamic;
