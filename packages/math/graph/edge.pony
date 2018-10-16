
class Edge
 var _source: Vertex
 var _destination: Vertex
 
 new create(src: Vertex, dst: Vertex) =>
  _source = src
  _destination = dst
  
 fun getSource(): Vertex => _source
 fun getDestination(): Vertex => _destination
