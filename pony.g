// ANTLR v3 grammar
grammar pony;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  : STRING? use* class_def* 
  ;

use
  : 'use' (ID '=')? (STRING | use_ffi) ('if' infix)?
  ;

use_ffi
  : '@' (ID | STRING) typeargs ('(' | LPAREN_NEW) params? ')' '?'?
  ;

class_def
  : ('type' | 'interface' | 'trait' | 'primitive' | 'class' | 'actor') '@'? ID typeparams? cap? ('is' types)? STRING? members
  ;

members
  : field* method*
  ;

field
  : ('var' | 'let') ID ':' type ('=' infix)?
  ;

method
  : ('fun' | 'be' | 'new') cap? ID typeparams? ('(' | LPAREN_NEW) params? ')' (':' type)? '?'? STRING? ('=>' rawseq)?
  ;

rawseq
  : exprseq
  | jump
  ;

exprseq
  : assignment (semiexpr | nosemi)?
  ;

nextexprseq
  : nextassignment (semiexpr | nosemi)?
  ;

nosemi
  : nextexprseq
  | jump
  ;

semiexpr
  : ';' (exprseq | jump)
  ;

jump
  : ('return' | 'break' | 'continue' | 'error' | 'compiler_intrinsic') rawseq?
  ;

nextassignment
  : nextinfix ('=' assignment)?
  ;

assignment
  : infix ('=' assignment)?
  ;

nextinfix
  : nextterm antrl_0*
  ;

infix
  : term antrl_1*
  ;

binop
  : ('and' | 'or' | 'xor' | '+' | '-' | '*' | '/' | '%' | '<<' | '>>' | 'is' | 'isnt' | '==' | '!=' | '<' | '<=' | '>=' | '>') term
  ;

nextterm
  : ('var' | 'let') ID (':' type)?
  | 'if' rawseq 'then' rawseq (elseif | ('else' rawseq))? 'end'
  | 'match' rawseq caseexpr* ('else' rawseq)? 'end'
  | 'while' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'repeat' rawseq 'until' rawseq ('else' rawseq)? 'end'
  | 'for' idseq 'in' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'with' (withelem (',' withelem)*) 'do' rawseq ('else' rawseq)? 'end'
  | 'try' rawseq ('else' rawseq)? ('then' rawseq)? 'end'
  | 'recover' cap? rawseq 'end'
  | 'consume' cap? term
  | ('not' | '&' | MINUS_NEW | 'identityof') term
  | nextpostfix
  ;

term
  : ('var' | 'let') ID (':' type)?
  | 'if' rawseq 'then' rawseq (elseif | ('else' rawseq))? 'end'
  | 'match' rawseq caseexpr* ('else' rawseq)? 'end'
  | 'while' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'repeat' rawseq 'until' rawseq ('else' rawseq)? 'end'
  | 'for' idseq 'in' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'with' (withelem (',' withelem)*) 'do' rawseq ('else' rawseq)? 'end'
  | 'try' rawseq ('else' rawseq)? ('then' rawseq)? 'end'
  | 'recover' cap? rawseq 'end'
  | 'consume' cap? term
  | ('not' | '&' | '-' | MINUS_NEW | 'identityof') term
  | postfix
  ;

withelem
  : idseq '=' rawseq
  ;

caseexpr
  : '|' infix? ('where' rawseq)? ('=>' rawseq)?
  ;

elseif
  : 'elseif' rawseq 'then' rawseq (elseif | ('else' rawseq))?
  ;

idseq
  : ID
  | '_'
  | ('(' | LPAREN_NEW) idseq (',' idseq)* ')'
  ;

nextpostfix
  : nextatom antrl_2*
  ;

postfix
  : atom antrl_3*
  ;

call
  : '(' positional? named? ')'
  ;

tilde
  : '~' ID
  ;

dot
  : '.' ID
  ;

nextatom
  : ID
  | literal
  | LPAREN_NEW (rawseq | '_') tuple? ')'
  | LSQUARE_NEW ('as' type ':')? rawseq (',' rawseq)* ']'
  | 'object' ('is' types)? members 'end'
  | '@' (ID | STRING) typeargs? ('(' | LPAREN_NEW) positional? named? ')' '?'?
  ;

atom
  : ID
  | literal
  | ('(' | LPAREN_NEW) (rawseq | '_') tuple? ')'
  | ('[' | LSQUARE_NEW) ('as' type ':')? rawseq (',' rawseq)* ']'
  | 'object' ('is' types)? members 'end'
  | '@' (ID | STRING) typeargs? ('(' | LPAREN_NEW) positional? named? ')' '?'?
  ;

literal
  : 'this'
  | 'true'
  | 'false'
  | INT
  | FLOAT
  | STRING
  ;

tuple
  : ',' (rawseq | '_') (',' (rawseq | '_'))*
  ;

positional
  : rawseq (',' rawseq)*
  ;

named
  : 'where' namedarg (',' namedarg)*
  ;

namedarg
  : ID '=' rawseq
  ;

type
  : atomtype ('->' type)?
  ;

atomtype
  : 'this'
  | 'box'
  | ('(' | LPAREN_NEW) (infixtype | '_') tupletype? ')'
  | nominal
  ;

tupletype
  : ',' (infixtype | '_') (',' (infixtype | '_'))*
  ;

infixtype
  : type antrl_4*
  ;

isecttype
  : '&' type
  ;

uniontype
  : '|' type
  ;

nominal
  : ID ('.' ID)? typeargs? cap? ('^' | '!')?
  ;

cap
  : 'iso'
  | 'trn'
  | 'ref'
  | 'val'
  | 'box'
  | 'tag'
  ;

typeargs
  : '[' type (',' type)* ']'
  ;

typeparams
  : ('[' | LSQUARE_NEW) typeparam (',' typeparam)* ']'
  ;

params
  : (param | '...') (',' (param | '...'))*
  ;

typeparam
  : ID (':' type)? ('=' type)?
  ;

param
  : ID ':' type ('=' infix)?
  ;

types
  : type (',' type)*
  ;

antrl_0
  : binop
  | 'as' type
  ;

antrl_1
  : binop
  | 'as' type
  ;

antrl_2
  : dot
  | tilde
  | typeargs
  | call
  ;

antrl_3
  : dot
  | tilde
  | typeargs
  | call
  ;

antrl_4
  : uniontype
  | isecttype
  ;

// Rules of the form antlr_* are only present to avoid a bug in the
// interpreter

/* Precedence

Value:
1. postfix
2. unop
3. binop
4. =
5. seq
6. ,

Type:
1. ->
2. & |
3. ,
*/

// Lexer

ID
  : LETTER (LETTER | DIGIT | '_' | '\'')*
  | '_' (LETTER | DIGIT | '_' | '\'')+
  ;

INT
  : DIGIT+
  | '0' 'x' HEX+
  | '0' 'b' BINARY+
  | '\'' CHAR_CHAR* '\''
  ;

FLOAT
  : DIGIT+ ('.' DIGIT+)? EXP?
  ;

STRING
  : '"' STRING_CHAR* '"'
  | '"""' ~('"""')* '"""'
  ;

LPAREN_NEW
  : NEWLINE '('
  ;

LSQUARE_NEW
  : NEWLINE '['
  ;

MINUS_NEW
  : NEWLINE '-'
  ;

LINECOMMENT
  : '//' ~('\n')* {$channel = HIDDEN;}
  ;

NESTEDCOMMENT
  : '/*' (NESTEDCOMMENT | '/' ~'*' | ~('*' | '/') | ('*'+ ~('*' | '/')))* '*'+ '/' {$channel = HIDDEN;}
  ;

WS
  : (' ' | '\t' | '\r')+ {$channel = HIDDEN;}
  ;

NEWLINE
  : '\n' (' ' | '\t' | '\r')* {$channel = HIDDEN;}
  ;

fragment
CHAR_CHAR
  : ESC
  | ~('\'' | '\\')
  ;

fragment
STRING_CHAR
  : ESC
  | ~('"' | '\\')
  ;

fragment
EXP
  : ('e' | 'E') ('+' | '-')? DIGIT+
  ;

fragment
LETTER
  : 'a'..'z' | 'A'..'Z'
  ;

fragment
BINARY
  : '0'..'1'
  ;

fragment
DIGIT
  : '0'..'9'
  ;

fragment
HEX
  : DIGIT | 'a'..'f' | 'A'..'F'
  ;

fragment
ESC
  : '\\' ('a' | 'b' | 'e' | 'f' | 'n' | 'r' | 't' | 'v' | '\"' | '\\' | '0')
  | HEX_ESC
  | UNICODE_ESC
  | UNICODE2_ESC
  ;

fragment
HEX_ESC
  : '\\' 'x' HEX HEX
  ;

fragment
UNICODE_ESC
  : '\\' 'u' HEX HEX HEX HEX
  ;

fragment
UNICODE2_ESC
  : '\\' 'U' HEX HEX HEX HEX HEX HEX
  ;

