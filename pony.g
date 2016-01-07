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
  : ('type' | 'interface' | 'trait' | 'primitive' | 'struct' | 'class' | 'actor') '@'? cap? ID typeparams? ('is' type)? STRING? members
  ;

members
  : field* method*
  ;

field
  : ('var' | 'let' | 'embed') ID ':' type ('delegate' type)? ('=' infix)?
  ;

method
  : ('fun' | 'be' | 'new') cap? ID typeparams? ('(' | LPAREN_NEW) params? ')' (':' type)? '?'? STRING? ('if' rawseq)? ('=>' rawseq)?
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
  : ('return' | 'break' | 'continue' | 'error' | 'compile_intrinsic' | 'compile_error') rawseq?
  ;

nextassignment
  : nextinfix ('=' assignment)?
  ;

assignment
  : infix ('=' assignment)?
  ;

nextinfix
  : nextterm antlr_0*
  ;

infix
  : term antlr_1*
  ;

binop
  : ('and' | 'or' | 'xor' | '+' | '-' | '*' | '/' | '%' | '<<' | '>>' | 'is' | 'isnt' | '==' | '!=' | '<' | '<=' | '>=' | '>') term
  ;

nextterm
  : 'if' rawseq 'then' rawseq (elseif | ('else' rawseq))? 'end'
  | 'ifdef' infix 'then' rawseq (elseifdef | ('else' rawseq))? 'end'
  | 'match' rawseq caseexpr* ('else' rawseq)? 'end'
  | 'while' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'repeat' rawseq 'until' rawseq ('else' rawseq)? 'end'
  | 'for' idseq 'in' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'with' (withelem (',' withelem)*) 'do' rawseq ('else' rawseq)? 'end'
  | 'try' rawseq ('else' rawseq)? ('then' rawseq)? 'end'
  | 'recover' cap? rawseq 'end'
  | 'consume' cap? term
  | nextpattern
  ;

term
  : 'if' rawseq 'then' rawseq (elseif | ('else' rawseq))? 'end'
  | 'ifdef' infix 'then' rawseq (elseifdef | ('else' rawseq))? 'end'
  | 'match' rawseq caseexpr* ('else' rawseq)? 'end'
  | 'while' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'repeat' rawseq 'until' rawseq ('else' rawseq)? 'end'
  | 'for' idseq 'in' rawseq 'do' rawseq ('else' rawseq)? 'end'
  | 'with' (withelem (',' withelem)*) 'do' rawseq ('else' rawseq)? 'end'
  | 'try' rawseq ('else' rawseq)? ('then' rawseq)? 'end'
  | 'recover' cap? rawseq 'end'
  | 'consume' cap? term
  | pattern
  ;

withelem
  : idseq '=' rawseq
  ;

caseexpr
  : '|' pattern? ('if' rawseq)? ('=>' rawseq)?
  ;

elseifdef
  : 'elseif' infix 'then' rawseq (elseifdef | ('else' rawseq))?
  ;

elseif
  : 'elseif' rawseq 'then' rawseq (elseif | ('else' rawseq))?
  ;

idseq
  : ID
  | '_'
  | ('(' | LPAREN_NEW) idseq (',' idseq)* ')'
  ;

nextpattern
  : ('var' | 'let' | 'embed') ID (':' type)?
  | nextparampattern
  ;

pattern
  : ('var' | 'let' | 'embed') ID (':' type)?
  | parampattern
  ;

nextparampattern
  : ('not' | 'addressof' | MINUS_NEW | 'identityof') parampattern
  | nextpostfix
  ;

parampattern
  : ('not' | 'addressof' | '-' | MINUS_NEW | 'identityof') parampattern
  | postfix
  ;

nextpostfix
  : nextatom antlr_2*
  ;

postfix
  : atom antlr_3*
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
  | 'this'
  | literal
  | LPAREN_NEW (rawseq | '_') tuple? ')'
  | LSQUARE_NEW ('as' type ':')? rawseq (',' rawseq)* ']'
  | 'object' cap? ('is' type)? members 'end'
  | 'lambda' cap? typeparams? ('(' | LPAREN_NEW) params? ')' lambdacaptures? (':' type)? '?'? '=>' rawseq 'end'
  | '@' (ID | STRING) typeargs? ('(' | LPAREN_NEW) positional? named? ')' '?'?
  ;

atom
  : ID
  | 'this'
  | literal
  | ('(' | LPAREN_NEW) (rawseq | '_') tuple? ')'
  | ('[' | LSQUARE_NEW) ('as' type ':')? rawseq (',' rawseq)* ']'
  | 'object' cap? ('is' type)? members 'end'
  | 'lambda' cap? typeparams? ('(' | LPAREN_NEW) params? ')' lambdacaptures? (':' type)? '?'? '=>' rawseq 'end'
  | '@' (ID | STRING) typeargs? ('(' | LPAREN_NEW) positional? named? ')' '?'?
  ;

tuple
  : ',' (rawseq | '_') (',' (rawseq | '_'))*
  ;

lambdacaptures
  : ('(' | LPAREN_NEW) lambdacapture (',' lambdacapture)* ')'
  ;

lambdacapture
  : ID (':' type)? ('=' infix)?
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
  | cap
  | ('(' | LPAREN_NEW) (infixtype | '_') tupletype? ')'
  | nominal
  ;

tupletype
  : ',' (infixtype | '_') (',' (infixtype | '_'))*
  ;

infixtype
  : type antlr_4*
  ;

isecttype
  : '&' type
  ;

uniontype
  : '|' type
  ;

nominal
  : ID ('.' ID)? typeargs? (cap | gencap)? ('^' | '!')?
  ;

gencap
  : '#read'
  | '#send'
  | '#share'
  | '#any'
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
  : '[' (type | literal | ('#' postfix)) (',' (type | literal | ('#' postfix)))* ']'
  ;

literal
  : 'true'
  | 'false'
  | INT
  | FLOAT
  | STRING
  ;

typeparams
  : ('[' | LSQUARE_NEW) (typeparam | typeparamvalue) (',' (typeparam | typeparamvalue))* ']'
  ;

params
  : (param | '...') (',' (param | '...'))*
  ;

typeparamvalue
  : 'let' ID (':' type)? ('=' infix)?
  ;

typeparam
  : ID (':' type)? ('=' type)?
  ;

param
  : (parampattern | '_') (':' type)? ('=' infix)?
  ;

antlr_0
  : binop
  | 'as' type
  ;

antlr_1
  : binop
  | 'as' type
  ;

antlr_2
  : dot
  | tilde
  | typeargs
  | call
  ;

antlr_3
  : dot
  | tilde
  | typeargs
  | call
  ;

antlr_4
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
  : DIGIT (DIGIT | '_')*
  | '0' 'x' (HEX | '_')+
  | '0' 'b' (BINARY | '_')+
  | '\'' CHAR_CHAR* '\''
  ;

FLOAT
  : DIGIT (DIGIT | '_')* ('.' DIGIT (DIGIT | '_')*)? EXP?
  ;

STRING
  : '"' STRING_CHAR* '"'
  | '"""' (('"' | '""') ? ~'"')* '"""' '"'*
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
  : ('e' | 'E') ('+' | '-')? (DIGIT | '_')+
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

