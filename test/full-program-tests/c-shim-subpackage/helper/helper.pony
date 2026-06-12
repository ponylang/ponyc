use "cdefine:HELPER_VALUE=2"

use @helper_value[I32]()

primitive Helper
  fun value(): I32 => @helper_value()
