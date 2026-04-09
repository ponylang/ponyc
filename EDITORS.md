# Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)
* Atom: [language-pony](https://atom.io/packages/language-pony)
* Visual Studio: [VS-pony](https://github.com/ponylang/VS-pony)
* Visual Studio Code: [ponylang-vscode-extension](https://marketplace.visualstudio.com/items?itemName=ponylang.ponylang-vscode-extension)
* Vim / Neovim:
  * [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig/blob/master/doc/configs.md#pony_lsp)
  * [vim-pony](https://github.com/jakwings/vim-pony)
  * [pony.vim](https://github.com/dleonard0/pony-vim-syntax)
  * [currycomb: Syntastic support](https://github.com/killerswan/pony-currycomb.vim)
  * [SpaceVim](http://spacevim.org), available as layer for Vim and [Neovim](https://neovim.io). Just follow [installation instructions](https://github.com/SpaceVim/SpaceVim) then load `lang#pony` layer inside configuration file (*$HOME/.SpaceVim.d/init.toml*)
* Emacs:
  * [ponylang-mode](https://github.com/ponylang/ponylang-mode)
  * [flycheck-pony](https://github.com/ponylang/flycheck-pony)
  * [pony-snippets](https://github.com/ponylang/pony-snippets)
* BBEdit: [bbedit-pony](https://github.com/TheMue/bbedit-pony)
* Micro: [micro-pony-plugin](https://github.com/Theodus/micro-pony-plugin)
* Nano: [pony.nanorc file](https://github.com/serialhex/nano-highlight/blob/master/pony.nanorc)
* Kate: update syntax definition file: Settings -> Configure Kate -> Open/Save -> Modes & Filetypes -> Download Highlighting Files
* CudaText: lexer in Addon Manager
* Vis: has a [lexer](https://github.com/martanne/vis/blob/master/lua/lexers/pony.lua) for Pony
* Kakoune: has [Pony support](https://github.com/mawww/kakoune/blob/master/rc/filetype/pony.kak) in the main repository
* Zed: [pony-zed](https://github.com/orien/pony-zed)
* Helix: ponylang support is builtin

## Language Server support

The ponylang language server implementation is [pony-lsp](https://www.ponylang.io/use/pony-lsp/). If your editor supports the [LSP protocol](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) please check [our documentation](https://www.ponylang.io/use/pony-lsp/) to setup the pony-lsp in your editor of choice.

## Treesitter support

If your editor supports treesitter grammar for navigation, hightlighting, navigation, syntax checking etc. you can use the official [ponylang treesitter grammar](https://github.com/tree-sitter-grammars/tree-sitter-pony).
