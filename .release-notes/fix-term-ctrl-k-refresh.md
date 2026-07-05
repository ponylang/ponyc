## Fix `term` leaving deleted text on screen after ctrl-k

In a `Readline` prompt, ctrl-k deletes from the cursor to the end of the line, but the line was not redrawn afterward, so the deleted text stayed on screen until the next keystroke that happened to redraw the line. ctrl-k now redraws the line, so the deleted text disappears right away.
