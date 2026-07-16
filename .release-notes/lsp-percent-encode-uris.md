## Fix pony-lsp not working when your editor escapes characters in a file path

When your editor tells a language server which file you are editing, it escapes any character that is not legal in a URI. A space is sent as `%20`, `é` as `%C3%A9`. VS Code and emacs lsp-mode also escape the colon after a Windows drive letter, sending `C:` as `C%3A`.

pony-lsp did not undo that escaping. It used the escaped text as the file name, so it looked for a file whose name really did contain `%20`, found nothing, and returned an error for every request about that file. If your project path contained a space or a non-ASCII character, pony-lsp did not work on it. That was true on every platform. On Windows with VS Code, pony-lsp did not work on any project at all, because of the escaped colon after the drive letter.

pony-lsp also did not escape the paths it sent back, so a path containing a space produced a location that was not a valid URI.

pony-lsp now undoes the escaping in the URIs your editor sends, and escapes the paths it sends back. There is nothing you need to change.
