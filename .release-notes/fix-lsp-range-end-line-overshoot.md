## Fix LSP range end positions overshooting past source line ends

Go-to-definition, document symbols, workspace symbols, type hierarchy, call hierarchy, and selection ranges could highlight text past the end of the declaration line. Editors that rely on this range for highlighting or cursor placement would overshoot into whitespace or the next token. This is now fixed.
