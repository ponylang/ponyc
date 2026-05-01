"""
Test fixtures for exercising LSP workspace/symbol functionality.

  _ws_sym_host.pony — an actor + trailing class covering every member
    token (`tk_new`, `tk_fun`, `tk_be`, `tk_flet`, `tk_fvar`, `tk_embed`)
    plus two top-level entity tokens (`tk_actor`, `tk_class`). Drives
    the match-semantics tests (exact / substring / case / container /
    empty / no-match) and the range test.
"""
