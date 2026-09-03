## Fix PonyCheck collection generators when min > max

PonyCheck's collection and string generators silently produced wrong-sized output when `min` was greater than `max`. Passing `min = 10, max = 5` generated collections from an arbitrarily large range instead of producing collections with 5 to 10 elements. Inverted arguments now produce the same result as the correctly ordered equivalent.
