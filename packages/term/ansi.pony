primitive ANSI
  """
  These strings can be embedded in text when writing to a StdStream to create
  a text-based UI.
  """
  fun up(n: U32 = 0): String =>
    """
    Move the cursor up n lines. 0 is the same as 1.
    """
    if n <= 1 then
      "\x1B[A"
    else
      "\x1B[" + n.string() + "A"
    end

  fun down(n: U32 = 0): String =>
    """
    Move the cursor down n lines. 0 is the same as 1.
    """
    if n <= 1 then
      "\x1B[B"
    else
      "\x1B[" + n.string() + "B"
    end

  fun right(n: U32 = 0): String =>
    """
    Move the cursor right n columns. 0 is the same as 1.
    """
    if n <= 1 then
      "\x1B[C"
    else
      "\x1B[" + n.string() + "C"
    end

  fun left(n: U32 = 0): String =>
    """
    Move the cursor left n columns. 0 is the same as 1.
    """
    if n <= 1 then
      "\x1B[D"
    else
      "\x1B[" + n.string() + "D"
    end

  fun cursor(x: U32 = 0, y: U32 = 0): String =>
    """
    Move the cursor to line y, column x. 0 is the same as 1. This indexes from
    the top left corner of the screen.
    """
    if (x <= 1) and (y <= 1) then
      "\x1B[H"
    else
      "\x1B[" + y.string() + ";" + x.string() + "H"
    end

  fun clear(): String =>
    """
    Clear the screen and move the cursor to the top left corner.
    """
    "\x1B[H\x1B[2J"

  fun erase(): String =>
    """
    Erases everything to the left of the cursor on the line the cursor is on.
    """
    "\x1B[0K"

  fun reset(): String =>
    """
    Resets all colours and text styles to the default.
    """
    "\x1B[0m"

  fun bold(state: Bool = true): String =>
    """
    Bold text. Does nothing on Windows.
    """
    if state then "\x1B[1m" else "\x1B[22m" end

  fun underline(state: Bool = true): String =>
    """
    Underlined text. Does nothing on Windows.
    """
    if state then "\x1B[4m" else "\x1B[24m" end

  fun blink(state: Bool = true): String =>
    """
    Blinking text. Does nothing on Windows.
    """
    if state then "\x1B[5m" else "\x1B[25m" end

  fun reverse(state: Bool = true): String =>
    """
    Swap foreground and background colour.
    """
    if state then "\x1B[7m" else "\x1B[27m" end

  fun black(): String =>
    """
    Black text.
    """
    "\x1B[30m"

  fun red(): String =>
    """
    Red text.
    """
    "\x1B[31m"

  fun green(): String =>
    """
    Green text.
    """
    "\x1B[32m"

  fun yellow(): String =>
    """
    Yellow text.
    """
    "\x1B[33m"

  fun blue(): String =>
    """
    Blue text.
    """
    "\x1B[34m"

  fun magenta(): String =>
    """
    Magenta text.
    """
    "\x1B[35m"

  fun cyan(): String =>
    """
    Cyan text.
    """
    "\x1B[36m"

  fun grey(): String =>
    """
    Grey text.
    """
    "\x1B[90m"

  fun white(): String =>
    """
    White text.
    """
    "\x1B[97m"

  fun bright_red(): String =>
    """
    Bright red text.
    """
    "\x1B[91m"

  fun bright_green(): String =>
    """
    Bright green text.
    """
    "\x1B[92m"

  fun bright_yellow(): String =>
    """
    Bright yellow text.
    """
    "\x1B[93m"

  fun bright_blue(): String =>
    """
    Bright blue text.
    """
    "\x1B[94m"

  fun bright_magenta(): String =>
    """
    Bright magenta text.
    """
    "\x1B[95m"

  fun bright_cyan(): String =>
    """
    Bright cyan text.
    """
    "\x1B[96m"

  fun bright_grey(): String =>
    """
    Bright grey text.
    """
    "\x1B[37m"

  fun black_bg(): String =>
    """
    Black background.
    """
    "\x1B[40m"

  fun red_bg(): String =>
    """
    Red background.
    """
    "\x1B[41m"

  fun green_bg(): String =>
    """
    Green background.
    """
    "\x1B[42m"

  fun yellow_bg(): String =>
    """
    Yellow background.
    """
    "\x1B[43m"

  fun blue_bg(): String =>
    """
    Blue background.
    """
    "\x1B[44m"

  fun magenta_bg(): String =>
    """
    Magenta background.
    """
    "\x1B[45m"

  fun cyan_bg(): String =>
    """
    Cyan background.
    """
    "\x1B[46m"

  fun grey_bg(): String =>
    """
    Grey background.
    """
    "\x1B[100m"

  fun white_bg(): String =>
    """
    White background.
    """
    "\x1B[107m"

  fun bright_red_bg(): String =>
    """
    Bright red background.
    """
    "\x1B[101m"

  fun bright_green_bg(): String =>
    """
    Bright green background.
    """
    "\x1B[102m"

  fun bright_yellow_bg(): String =>
    """
    Bright yellow background.
    """
    "\x1B[103m"

  fun bright_blue_bg(): String =>
    """
    Bright blue background.
    """
    "\x1B[104m"

  fun bright_magenta_bg(): String =>
    """
    Bright magenta background.
    """
    "\x1B[105m"

  fun bright_cyan_bg(): String =>
    """
    Bright cyan background.
    """
    "\x1B[106m"

  fun bright_grey_bg(): String =>
    """
    Bright grey background.
    """
    "\x1B[47m"
