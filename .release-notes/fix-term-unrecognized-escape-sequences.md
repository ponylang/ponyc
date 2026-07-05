## Fix `term` mishandling of unrecognized escape sequences

The `term` package's input decoder mishandled terminal escape sequences it didn't recognize. An unrecognized sequence such as Shift-Tab (`ESC[Z`) leaked stray characters into the input; an unknown keypad code stalled the decoder so following keystrokes were dropped or garbled; and an out-of-range numeric parameter could be read as a different, valid key.

The decoder now reads each escape sequence through to its end and discards it when it isn't recognized, so an unrecognized key no longer inserts stray text, stalls input, or fires the wrong key. Recognized keys are unaffected.
