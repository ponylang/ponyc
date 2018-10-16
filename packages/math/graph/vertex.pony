
class Vertex[Value: Any val]
  var _value: Value
  
  new create(value: Value) =>
    _value = value
    
  fun get(): Value => _value

  fun ref set(value: Value) => _value = value
