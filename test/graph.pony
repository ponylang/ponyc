use "collections"

trait Graph[G: Graph[G, E, V], E: Edge[G, E, V], V: Vertex[G, E, V]]
  fun box getVertices(): this->List[V]

trait Edge[G: Graph[G, E, V], E: Edge[G, E, V], V: Vertex[G, E, V]]
  fun box getGraph(): this->G
  fun box getSource(): this->V
  fun box getTarget(): this->V

trait Vertex[G: Graph[G, E, V], E: Edge[G, E, V], V: Vertex[G, E, V]]
  fun box getGraph(): this->G
  fun box getIncoming(): this->List[E]
  fun box getOutgoing(): this->List[E]

class Map is Graph[Map, Road, City]
  var cities: List[City]
  fun box getVertices(): this->List[City] => cities

class Road is Edge[Map, Road, City]
  var map: Map
  var source: City
  var target: City

  fun box getGraph(): this->Map => map
  fun box getSource(): this->City => source
  fun box getTarget(): this->City => target

class City is Vertex[Map, Road, City]
  var map: Map
  var incoming: List[Road]
  var outgoing: List[Road]

  fun box getGraph(): this->Map => map
  fun box getIncoming(): this->List[Road] => incoming
  fun box getOutgoing(): this->List[Road] => outgoing
