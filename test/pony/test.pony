//use "collections" as c
//use "patterns" as p
//use "precedence" as prec

class Foo
	fun box bar(x: U32):U32 =>
		match(x)
		| 4 where x > 3 => -3
		else
			1 + 2 * 3 - 4 * 5
		end
