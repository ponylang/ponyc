"""
# CLI Package

The CLI package provides enhanced Posix+GNU command line parsing with the
feature of commands that can be specified in a hierarchy.

See [RFC-0038](https://github.com/ponylang/rfcs/blob/master/text/0038-cli-format.md) for more background.

The general EBNF of a command line is:
```ebnf
  command_line ::= root_command (option | command)* (option | arg)*
  command ::= alphanum_word
  alphanum_word ::= alphachar(alphachar | numchar | '_' | '-')*
  option ::= longoption | shortoptionset
  longoption ::= '--'alphanum_word['='arg | ' 'arg]
  shortoptionset := '-'alphachar[alphachar]...['='arg | ' 'arg]
  arg := boolarg | intarg | floatarg | stringarg
  boolarg := 'true' | 'false'
  intarg> := ['-'] numchar...
  floatarg ::= ['-'] numchar... ['.' numchar...]
  stringarg ::= anychar
```

Some examples:
```
  usage: ls [<options>] [<args> ...]
  usage: make [<options>] <command> [<options>] [<args> ...]
  usage: chat [<options>] <command>  <subcommand> [<options>] [<args> ...]
```

## Usage

The types in the cli package are broken down into three groups:

### Specs

Pony programs use constructors to create the spec objects to specify their
command line syntax. Many aspects of the spec are checked for correctness at
compile time, and the result represents everything the parser needs to know
when parsing a command line or forming syntax help messages.

#### Option and Arg value types

Options and Args parse values from the command line as one of four Pony types:
`Bool`, `String`, `I64` and `F64`. Values of each of these types can then be
retrieved using the corresponding accessor funtions.

In addition, there is a string_seq type that accepts string values from the
command line and collects them into a sequence which can then be retrieved as
a `ReadSeq[String]` using the `string_seq()` accessor function.

Some specific details:

- bool Options: have a default value of 'true' if no value is given. That is,
  `-f` is equivalent to `-f=true`.

- string_seq Options: the option prefix has to be used each time, like:
  `--file=f1 --file=f2 --file=f3` with the results being collected into
  a single sequence.

- string_seq Args: there is no way to indicate termination, so a string_seq
  Arg should be the last arg for a command, and will consume all remaining
  command line arguments.

### Parser

Programs then use the CommandSpec they've built to instantiate a parser to
parse any given command line. This is often env.args(), but could also be
commands from files or other input sources. The result of a parse is either a
parsed command, a command help, or a syntax error object.

### Commands

Programs then match the object returned by the parser to determine what kind
it is. Errors and help requests typically print messages and exit. For
commands, the fullname can be matched and the effective values for the
command's options and arguments can be retrieved.

# Example program

This program echos its command line arguments with the option of uppercasing
them.

```pony
use "cli"

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf("echo", "A sample echo program", [
          OptionSpec.bool("upper", "Uppercase words"
            where short' = 'U', default' = false)
        ], [
          ArgSpec.string_seq("words", "The words to echo")
        ])? .> add_help()?
      else
        env.exitcode(-1)  // some kind of coding error
        return
      end

    let cmd =
      match CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
          ch.print_help(env.out)
          env.exitcode(0)
          return
      | let se: SyntaxError =>
          env.out.print(se.string())
          env.exitcode(1)
          return
      end

    let upper = cmd.option("upper").bool()
    let words = cmd.arg("words").string_seq()
    for word in words.values() do
      env.out.write(if upper then word.upper() else word end + " ")
    end
    env.out.print("")
```
"""
