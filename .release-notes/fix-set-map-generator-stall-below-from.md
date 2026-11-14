## Fix set and map generators silently returning fewer than `from` elements

The `set_of`, `set_is_of`, `map_of`, `map_is_of`, `persistent_set_of`, `persistent_set_is_of`, `persistent_map_of`, and `persistent_map_is_of` generators could return a collection smaller than `from` when the element generator couldn't produce enough distinct values. The generators now error instead, which the property runner retries and then fails visibly.
