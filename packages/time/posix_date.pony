use @ponyint_gmtime[None](date: PosixDate, sec: I64, nsec: I64)
use @ponyint_timegm[I64](date: PosixDate tag)
use @ponyint_formattime[Pointer[U8]](date: PosixDate tag, fmt: Pointer[U8] tag) ?

class PosixDate
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
    Create a date from a POSIX time. Negative arguments will be changed to zero.
    """
    @ponyint_gmtime(this,
      _negative_to_zero(seconds),
      _negative_to_zero(nanoseconds))

  fun time(): I64 =>
    """
    Return a POSIX time. Treats the date as UTC.
    """
    @ponyint_timegm(this)

  fun ref normal() =>
    """
    Normalise all the fields of the date. For example, if the hour is 24, it is
    set to 0 and the day is advanced. This allows fields to be changed naively,
    eg. adding 1000 to hours to advance the time by 1000 hours, and then
    normalising the date.
    """
    @ponyint_gmtime(this, time(), nsec.i64())

  fun format(fmt: String): String ? =>
    """
    Format the time as for strftime.
    """
    recover
      String.from_cstring(@ponyint_formattime(this, fmt.cstring())?)
    end

  fun _negative_to_zero(value: I64): I64 =>
    if value > 0 then
      value
    else
      0
    end
