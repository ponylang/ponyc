## Fix LSP parameter hover to show valid Pony syntax

Hovering over a method parameter in the LSP previously showed `param name: String` — a `param` keyword that does not exist in Pony. It now shows `name: String`, which is the correct representation of a parameter.
