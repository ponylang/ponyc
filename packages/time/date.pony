class Date
  """
  Represents a proleptic Gregorian date and time, without specifying a
  time zone. The day of month, month, day of week, and day of year are all
  indexed from 1, i.e. January is 1, Monday is 1.
  """
  var nsec: I32 = 0
  var sec: I32 = 0
  var min: I32 = 0
  var hour: I32 = 0
  var day_of_month: I32 = 1
  var month: I32 = 1
  var year: I32 = 1970
  var day_of_week: I32 = 4
  var day_of_year: I32 = 1

  new create(seconds: I64 = 0, nanoseconds: I64 = 0) =>
    """
    Create a date from a POSIX time.
    """
    @ponyint_gmtime[None](this, seconds, nanoseconds)

  fun time(): I64 =>
    """
    Return a POSIX time. Treats the date as UTC.
    """
    @ponyint_timegm[I64](this)

  fun ref normal() =>
    """
    Normalise all the fields of the date. For example, if the hour is 24, it is
    set to 0 and the day is advanced. This allows fields to be changed naively,
    eg. adding 1000 to hours to advance the time by 1000 hours, and then
    normalising the date.
    """
    @ponyint_gmtime[None](this, time(), nsec)

  fun format(fmt: String): String =>
    """
    Format the time as for strftime.
    """
    recover
      String.from_cstring(@ponyint_formattime[Pointer[U8]](this, fmt.cstring()))
    end
