## Fix spurious LSP inlay hints inside lambda type annotations

Nominal types appearing inside lambda type annotations (e.g. `{(String): None} val` or `{(): String} val`) were causing spurious capability inlay hints at incorrect source positions — for example, a ` val` hint would appear in the middle of a type name. These hints no longer appear.
