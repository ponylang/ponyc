
class Edge
 var _source: Vertex[Value: Any val]
 var _destination: Vertext[Value: Any val]
 
 new create(src: Vertex, dst: Vertex) =>
  _source = src
  _destination = dst
  
 fun getSource(): Vertex => _source
 fun getDestination(): Vertex => _destination
