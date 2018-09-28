primitive NumericArrayConverter
  fun convert_array[A: Real[A] val, B: Real[B] val](arr: Array[A val] box): Array[B] box
  =>
    """
    Create a new Box array pointing to the same memeory as the existing array but
    interpreting the data as a different numeric data type. The contents are not
    copied. Additionally, there are no partial elements allowed (i.e. a 2 byte U8
    array will become a 0 byte U32 array because a U32 requires 4 bytes).
    """
    let b_ptr = Pointer[B]
    let converted_size = (arr.size() * arr._element_size()) / b_ptr._element_size()
    let converted_array = recover Array[B].from_cpointer(arr.cpointer()._unsafe()._convert[B](), converted_size) end
    consume converted_array
