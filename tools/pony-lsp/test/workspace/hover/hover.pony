"""
Test fixtures for exercising LSP hover functionality.

This module provides test cases for manual and automated testing of hover
functionality provided by the Pony language server (LSP). It contains various
Pony entities (classes, actors, traits, methods, fields) with docstrings and
type annotations to validate hover behavior.

To manually test hover functionality:
1. Open the lsp/test/workspace directory as a project in your editor.
2. Open the various files n the hover directory while the Pony language server
   is active.
3. Hover your cursor over various code elements to see hover information.

Expected hover behavior:
- Display the declaration signature in a code block
- Include docstrings where present
- Show type information for fields and variables
"""
