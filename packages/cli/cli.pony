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

### Parser

Programs then use the spec to create a parser at runtime to parse any given
command line. This is often env.args(), but could also be commands from files
or other input sources. The result of a parse is either a parsed command, a
help command object, or a syntax error.

### Commands

Programs then query the returned parsed command to determine the command
specified, and the effective values for all options and arguments.


# Example program

This program opens the files that are given as command line arguments
and prints their contents.

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
        ]).>add_help()
      else
        env.exitcode(-1)  // some kind of coding error
        return
      end

    let cmd =
      match CommandParser(cs).parse(env.args, env.vars())
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
