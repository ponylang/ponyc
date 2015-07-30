trait TestList
  """
  Source of unit tests for a PonyTest object.
  See package doc string for further information and example use.
  """

  fun tag tests(test: PonyTest)
    """
    Add all the tests in this suite to the given test object.
    Typically the implementation of this function will be of the form:
    ```
    fun tests(test: PonyTest) =>
      test(_TestClass1)
      test(_TestClass2)
      test(_TestClass3)
    ```
    """
