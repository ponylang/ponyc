## Report fileless compiler errors without invalid diagnostic URIs

The language server now reports compiler errors that have no source file as editor messages, including their related explanations. These errors no longer appear as document diagnostics under invalid URIs such as `file://`.
