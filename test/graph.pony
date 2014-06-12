use "collections"

trait Graph[G: Graph[G, E, V], E: Edge[G, E, V], V: Vertex[G, E, V]]
  fun box getVertices(): List[V] this

trait Edge[G: Graph[G, E, V], E: Edge[G, E, V], V: Vertex[G, E, V]]
  fun box getGraph(): G this
  fun box getSource(): V this
  fun box getTarget(): V this

trait Vertex[G: Graph[G, E, V], E: Edge[G, E, V], V: Vertex[G, E, V]]
  fun box getGraph(): G this
  fun box getIncoming(): List[E] this
  fun box getOutgoing(): List[E] this

class Map is Graph[Map, Road, City]
  var cities: List[City]
  fun box getVertices(): List[City] this => cities

class Road is Edge[Map, Road, City]
  var map: Map
  var source: City
  var target: City

  fun box getGraph(): Map this => map
  fun box getSource(): City this => source
  fun box getTarget(): City this => target

class City is Vertex[Map, Road, City]
  var map: Map
  var incoming: List[Road]
  var outgoing: List[Road]

  fun box getGraph(): Map this => map
  fun box getIncoming(): List[Road] this => incoming
  fun box getOutgoing(): List[Road] this => outgoing
