## Fix Windows programs never finishing when stdin has ended

On Windows, a program that subscribed to stdin after stdin had ended could hang forever at 0% CPU. The new subscription never received a read, so the end of the input never reached the program, and the program never exited. A program run with its input redirected from `NUL` had a matching problem: the end of its input never arrived either, and it spun at 100% CPU instead of exiting.

A Windows program reading redirected stdin now reaches the end of its input the way it does on Linux and macOS.

## Fix Windows programs stalling while reading stdin from a pipe

On Windows, a read of stdin from a pipe did not return until the program on the other end of the pipe wrote a byte or closed the pipe. With one scheduler thread, nothing else in the program ran in the meantime: no timer fired and no other actor ran. With the default number of scheduler threads, the rest of the program kept running, but `env.input.dispose()` had no effect until the other end wrote a byte or closed the pipe, so the program kept reading stdin and could not exit.

The rest of a Windows program now runs while it waits for input on stdin, and `env.input.dispose()` takes effect when you call it, as it already did on Linux and macOS. Console input, stdin redirected from a file, and stdin from NUL were never affected.

## Fix a new Windows stdin notifier getting a read posted for the one it replaced

On Windows, a program that disposed its stdin notifier and installed a new one could have the new notifier sent a read that was posted for the notifier it replaced.

Reading stdin through `env.input` and the standard library's `Stdin` actor, nothing was lost, duplicated, or reordered, so most programs saw no difference. A program that subscribed to stdin readiness through the asio event API directly could be sent a read posted for a notifier it had already replaced. A new notifier is now sent only the reads posted for it.

## Fix `F32.ldexp` linking on Windows

Any program that called `F32.ldexp` failed to build on Windows:

```console
unable to link: lld-link: error: undefined symbol: ldexpf
```

This has been fixed.

## Fix pony-lsp not working when your editor escapes characters in a file path

When your editor tells a language server which file you are editing, it escapes any character that is not legal in a URI. A space is sent as `%20`, `é` as `%C3%A9`. VS Code and emacs lsp-mode also escape the colon after a Windows drive letter, sending `C:` as `C%3A`.

pony-lsp did not undo that escaping. It used the escaped text as the file name, so it looked for a file whose name really did contain `%20`, found nothing, and returned an error for every request about that file. If your project path contained a space or a non-ASCII character, pony-lsp did not work on it. That was true on every platform. On Windows with VS Code, pony-lsp did not work on any project at all, because of the escaped colon after the drive letter.

pony-lsp also did not escape the paths it sent back, so a path containing a space produced a location that was not a valid URI.

pony-lsp now undoes the escaping in the URIs your editor sends, and escapes the paths it sends back. There is nothing you need to change.

## Fix pony-lsp not working when your editor sends your project's location as a URI

When your editor starts a language server, it tells the server where your project is. Some editors send a list of folders. Others send a single location instead, either as a plain path like `/home/you/myproject` or as a URI like `file:///home/you/myproject`.

pony-lsp read that single location as a plain path in both cases. When an editor sent a URI, pony-lsp searched for a directory whose name really did start with `file://`, found nothing, and registered no project at all. Every request about your code returned an error, so hover, go to definition, find references, and rename all did nothing.

vim-lsp sends a URI in its default configuration, so pony-lsp did not work there on a stock setup. Kate sends one when you have configured a project root. If your editor sends a folder list with your project in it, as VS Code does, pony-lsp already worked.

pony-lsp now accepts a project location in either form. There is nothing you need to change.

One case is not covered by this fix: an editor that sends an empty folder list alongside a project location still finds no project. Kate does this when you have not configured a project root.

