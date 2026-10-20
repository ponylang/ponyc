## Fix JSONParser.parse accepting trailing incomplete tokens

`JSONParser.parse` accepted input containing a valid JSON value followed by an incomplete token — for example, `"nulln"` or `1 "x`. Instead of reporting an error, it silently ignored the trailing content and returned the first value. It now returns a parse error for any input where content remains after the first value.
