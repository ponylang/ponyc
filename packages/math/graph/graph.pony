use "collections/persistent"
"""
A Graph is defined as a Set vertices and a Set of Edges.
In pure Graph Theory, the vertex set MUST NOT be empty
but obviously that would be quite annoying, so we will
allow it, and provide an IsValidGraph method which will
return true IFF the vertex set is not the empty set.
"""
class Graph
  var _vertices: Lists[Vertex]
  var _edges: Lists[Edge]
  
  fun create() =>
    _vertices = Lists[Vertex].empty()
    _edges = Lists[Edge].empty()
    
  fun addVertex(vertex: Vertex): Bool ? =>
    _vertices = _vertices.prepend(vertex)
    true
    
  fun addEdge(edge: Edge): Bool ? =>
    _edges = _edges.prepend(edge)
    true
    
  fun isValidGraph(): Bool ? =>
    _vertices.is_empty()
  
