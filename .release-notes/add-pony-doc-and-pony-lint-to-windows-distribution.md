## Add pony-doc and pony-lint to Windows distribution

pony-doc and pony-lint were accidentally left out of the Windows distribution packages. Both tools were being compiled during the Windows build but then removed during packaging. They are now included alongside ponyc and pony-lsp.
