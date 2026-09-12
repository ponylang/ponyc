use json = "json"

interface val JSONDecoder[A: Any val]
  """
  Interface for converting a parsed `JSONValue` into a typed domain object.

  Implement this interface to define how a specific JSON structure maps to your
  application type. Return `JSONDecodeError` when the JSON doesn't match the
  expected structure.

  ```pony
  use "http_client"
  use json = "json"

  class val User
    let name: String
    let age: I64

    new val create(name': String, age': I64) =>
      name = name'
      age = age'

  primitive UserDecoder is JSONDecoder[User]
    fun apply(value: json.JSONValue): (User | JSONDecodeError) =>
      let nav = json.JSONNav(value)
      try
        User(nav("name").as_string()?, nav("age").as_i64()?)
      else
        JSONDecodeError("expected object with string 'name' and integer 'age'")
      end
  ```
  """

  fun apply(value: json.JSONValue): (A | JSONDecodeError)
    """
    Decode a parsed JSON value into a domain object, or return an error if
    the JSON structure doesn't match.
    """
