# PEG

Parser Expression Grammar compiler and combinators

## Installation

- Install [corral](https://github.com/ponylang/corral)
- `corral add github.com/ponylang/peg.git --version 0.1.5`
- `corral fetch` to fetch your dependencies
- `use "peg"` to include this package
- `corral run -- ponyc` to compile your application

## API Documentation

[https://ponylang.github.io/peg/](https://ponylang.github.io/peg/)

## Resources for PEGs

- [Wikipedia](https://en.wikipedia.org/wiki/Parsing_expression_grammar)
- [PEG Paper and Slides by Bryan Ford](https://bford.info/pub/lang/peg/)

## JSON Example

### Parser Compiler

A parser for JSON may be compiled from a PEG file such as the following:

```peg
// examples/json.peg

DIGIT19 <- '1'..'9'
DIGIT <- '0'..'9'
DIGITS <- DIGIT+
INT <- '-' DIGIT19 DIGITS / '-' DIGIT / DIGIT19 DIGITS / DIGIT
FRAC <- '.' DIGITS
EXP <- ('e' / 'E') ('-' / '+')? DIGITS
NUMBER <- INT FRAC? EXP?

HEX <- DIGIT / 'a'..'f' / 'A'..'F'
CHAR <-
  "\\\"" / "\\\\" / "\\/" / "\\b" / "\\f" / "\\n" / "\\r" / "\\t" /
  "\\u" HEX HEX HEX HEX / !"\"" !"\\" .
STRING <- "\"" CHAR* "\""
BOOL <- "true" / "false"

value <- "null" / BOOL / NUMBER / STRING / object / array
pair <- STRING -':' value

object <- -'{' (pair % ',') -'}'
array <- -'[' (value % ',') -']'

whitespace <- (' ' / '\t' / '\r' / '\n')+
linecomment <- "//" (!'\r' !'\n' .)*
nestedcomment <- "/*" ((!"/*" !"*/" .) / nestedcomment)* "*/"

start <- value
hidden <- (whitespace / linecomment / nestedcomment)+
```

### Combinators

A parser for JSON may also be constructed from combinators at compile time:

```pony
// peg/json.pony

primitive JsonParser
  fun apply(): Parser val =>
    recover
      let obj = Forward
      let array = Forward

      let digit19 = R('1', '9')
      let digit = R('0', '9')
      let digits = digit.many1()
      let int =
        (L("-") * digit19 * digits) / (L("-") * digit) /
        (digit19 * digits) / digit
      let frac = L(".") * digits
      let exp = (L("e") / L("E")) * (L("+") / L("-")).opt() * digits
      let number = (int * frac.opt() * exp.opt()).term(TNumber)

      let hex = digit / R('a', 'f') / R('A', 'F')
      let char =
        L("\\\"") / L("\\\\") / L("\\/") / L("\\b") / L("\\f") / L("\\n") /
        L("\\r") / L("\\t") / (L("\\u") * hex * hex * hex * hex) /
        (not L("\"") * not L("\\") * R(' '))

      let string = (L("\"") * char.many() * L("\"")).term(TString)
      let value =
        L("null").term(TNull) / L("true").term(TBool) / L("false").term(TBool) /
        number / string / obj / array
      let pair = (string * -L(":") * value).node(TPair)
      obj() = -L("{") * pair.many(L(",")).node(TObject) * -L("}")
      array() = -L("[") * value.many(L(",")).node(TArray) * -L("]")

      let whitespace = (L(" ") / L("\t") / L("\r") / L("\n")).many1()
      let linecomment =
        (L("#") / L("//")) * (not L("\r") * not L("\n") * Unicode).many()
      let nestedcomment = Forward
      nestedcomment() =
        L("/*") *
        ((not L("/*") * not L("*/") * Unicode) / nestedcomment).many() *
        L("*/")
      let hidden = (whitespace / linecomment / nestedcomment).many()

      value.hide(hidden)
    end

primitive TObject is Label fun text(): String => "Object"
primitive TPair is Label fun text(): String => "Pair"
primitive TArray is Label fun text(): String => "Array"
primitive TString is Label fun text(): String => "String"
primitive TNumber is Label fun text(): String => "Number"
primitive TBool is Label fun text(): String => "Bool"
primitive TNull is Label fun text(): String => "Null"
```

## Executable Parser Compiler

An executable example of the parser compiler is found in examples/compiler.
You can compile the executable by running `make build-examples`.

```console
Usage:
  ./build/release/compiler <source-file>
      Run a PEG/JSON parser over some source file and print the AST.
  ./build/release/compiler <peg-file> <source-file>
      Compile a parser from the first file, then run it over the second file and
      print the AST.
```
