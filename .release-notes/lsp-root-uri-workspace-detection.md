## Fix pony-lsp not working when your editor sends your project's location as a URI

When your editor starts a language server, it tells the server where your project is. Some editors send a list of folders. Others send a single location instead, either as a plain path like `/home/you/myproject` or as a URI like `file:///home/you/myproject`.

pony-lsp read that single location as a plain path in both cases. When an editor sent a URI, pony-lsp searched for a directory whose name really did start with `file://`, found nothing, and registered no project at all. Every request about your code returned an error, so hover, go to definition, find references, and rename all did nothing.

vim-lsp sends a URI in its default configuration, so pony-lsp did not work there on a stock setup. Kate sends one when you have configured a project root. If your editor sends a folder list with your project in it, as VS Code does, pony-lsp already worked.

pony-lsp now accepts a project location in either form. There is nothing you need to change.

One case is not covered by this fix: an editor that sends an empty folder list alongside a project location still finds no project. Kate does this when you have not configured a project root.
