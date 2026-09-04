## Fix PonyCheck set and map generators to guarantee requested element counts

The PonyCheck collection generators `set_of`, `set_is_of`, `map_of`, `map_is_of`, and their persistent variants treated `from` and `to` as the number of insertion attempts rather than the number of elements in the result. When the source generator produced duplicate values, the collection could end up smaller than `from`.

The generators now keep drawing from the source until the collection reaches the target size. If the source generator cannot produce enough distinct values (100 consecutive insertions with no growth), the generator stops and returns the collection at its current size.
