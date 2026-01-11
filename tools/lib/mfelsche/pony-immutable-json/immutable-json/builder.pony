use "collections"

interface JsonBuilder
  fun ref build(): JsonType

class Obj
  """
  JsonObject Builder

  ```pony
  let json_object = Obj("key", 14.5)("another-one", Obj("nested", None)).build()
  ```
  """
  var data: Map[String, JsonType] trn

  new ref create() =>
    data = Map[String, JsonType].create(4)

  fun ref apply(key: String, value: (JsonType | JsonBuilder)): Obj =>
    data(key) =
      match value
      | let t: JsonType => t
      | let builder: JsonBuilder => builder.build()
      end
    consume this

  fun ref build(): JsonObject =>
    JsonObject(this.data = Map[String, JsonType])


class Arr
  """
  JsonArray Builder

  ```pony
  let json_array = Arr("key")(14.5)(Obj("nested", None)).build()
  ```
  """
  var data: Array[JsonType] trn

  new ref create() =>
    data = Array[JsonType].create(4)

  fun ref apply(elem: (JsonType | JsonBuilder)): Arr ref^ =>
    data.push(
      match elem
      | let t: JsonType => t
      | let builder: JsonBuilder => builder.build()
      end
    )
    consume this

  fun ref build(): JsonArray =>
    JsonArray(this.data = Array[JsonType].create(0))

