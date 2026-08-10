## Readline preserves the in-progress line when browsing history

Browsing history with the up and down arrow keys in `Readline` no longer discards the line you were typing. Previously, pressing up replaced the current line with no backup, and pressing down at the newest entry cleared it. Edits made to a recalled history entry were also lost when navigating away and back.

The line you were typing reappears when you arrow back past the newest history entry. Edits to recalled entries stick while you browse and reset when you press Enter.
