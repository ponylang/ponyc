## Fix `Readline` nudging the cursor right on an empty line

When a `Readline` had an empty prompt and an empty line, refreshing the line moved the cursor one column to the right of where it belonged. It now stays at the left edge.
