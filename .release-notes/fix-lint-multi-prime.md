## Fix pony-lint false positive on identifiers with multiple trailing primes

pony-lint flagged identifiers like `path''` as naming violations because it only stripped one trailing prime before checking the name. An identifier with two or more primes kept the extras and failed the snake_case or CamelCase check. pony-lint now strips all trailing primes before validating.
