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
  : ('type' | 'interface' | 'trait' | 'primitive' | 'struct' | 'class' | 'actor') ('\' ID (',' ID)* '\')? '@'? cap? ID typeparams? ('is' type)? STRING? members
  ;

members
  : field* method*
  ;

field
  : ('var' | 'let' | 'embed') ID ':' type ('=' infix)?
  ;

method
  : ('fun' | 'be' | 'new') ('\' ID (',' ID)* '\')? cap? ID typeparams? ('(' | LPAREN_NEW) params? ')' (':' type)? '?'? STRING? ('if' rawseq)? ('=>' rawseq)?
  ;

annotatedrawseq
  : ('\' ID (',' ID)* '\')? (exprseq | jump)
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
  : ('and' | 'or' | 'xor' | '+' | '-' | '*' | '/' | '%' | '+~' | '-~' | '*~' | '/~' | '%~' | '<<' | '>>' | '<<~' | '>>~' | 'is' | 'isnt' | '==' | '!=' | '<' | '<=' | '>=' | '>' | '==~' | '!=~' | '<~' | '<=~' | '>=~' | '>~') term
  ;

nextterm
  : 'if' ('\' ID (',' ID)* '\')? rawseq 'then' rawseq (elseif | ('else' annotatedrawseq))? 'end'
  | 'ifdef' ('\' ID (',' ID)* '\')? infix 'then' rawseq (elseifdef | ('else' annotatedrawseq))? 'end'
  | 'match' ('\' ID (',' ID)* '\')? rawseq caseexpr* ('else' annotatedrawseq)? 'end'
  | 'while' ('\' ID (',' ID)* '\')? rawseq 'do' rawseq ('else' annotatedrawseq)? 'end'
  | 'repeat' ('\' ID (',' ID)* '\')? rawseq 'until' annotatedrawseq ('else' annotatedrawseq)? 'end'
  | 'for' ('\' ID (',' ID)* '\')? idseq 'in' rawseq 'do' rawseq ('else' annotatedrawseq)? 'end'
  | 'with' ('\' ID (',' ID)* '\')? (withelem (',' withelem)*) 'do' rawseq ('else' annotatedrawseq)? 'end'
  | 'try' ('\' ID (',' ID)* '\')? rawseq ('else' annotatedrawseq)? ('then' annotatedrawseq)? 'end'
  | 'recover' ('\' ID (',' ID)* '\')? cap? rawseq 'end'
  | 'consume' cap? term
  | nextpattern
  | '#' postfix
  ;

term
  : 'if' ('\' ID (',' ID)* '\')? rawseq 'then' rawseq (elseif | ('else' annotatedrawseq))? 'end'
  | 'ifdef' ('\' ID (',' ID)* '\')? infix 'then' rawseq (elseifdef | ('else' annotatedrawseq))? 'end'
  | 'match' ('\' ID (',' ID)* '\')? rawseq caseexpr* ('else' annotatedrawseq)? 'end'
  | 'while' ('\' ID (',' ID)* '\')? rawseq 'do' rawseq ('else' annotatedrawseq)? 'end'
  | 'repeat' ('\' ID (',' ID)* '\')? rawseq 'until' annotatedrawseq ('else' annotatedrawseq)? 'end'
  | 'for' ('\' ID (',' ID)* '\')? idseq 'in' rawseq 'do' rawseq ('else' annotatedrawseq)? 'end'
  | 'with' ('\' ID (',' ID)* '\')? (withelem (',' withelem)*) 'do' rawseq ('else' annotatedrawseq)? 'end'
  | 'try' ('\' ID (',' ID)* '\')? rawseq ('else' annotatedrawseq)? ('then' annotatedrawseq)? 'end'
  | 'recover' ('\' ID (',' ID)* '\')? cap? rawseq 'end'
  | 'consume' cap? term
  | pattern
  | '#' postfix
  ;

withelem
  : idseq '=' rawseq
  ;

caseexpr
  : '|' ('\' ID (',' ID)* '\')? pattern? ('if' rawseq)? ('=>' rawseq)?
  ;

elseifdef
  : 'elseif' ('\' ID (',' ID)* '\')? infix 'then' rawseq (elseifdef | ('else' annotatedrawseq))?
  ;

elseif
  : 'elseif' ('\' ID (',' ID)* '\')? rawseq 'then' rawseq (elseif | ('else' annotatedrawseq))?
  ;

idseq
  : ID
  | ('(' | LPAREN_NEW) idseq_in_seq (',' idseq_in_seq)* ')'
  ;

idseq_in_seq
  : ID
  | ('(' | LPAREN_NEW) idseq_in_seq (',' idseq_in_seq)* ')'
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
  : ('not' | 'addressof' | MINUS_NEW | '-~' | 'digestof') parampattern
  | nextpostfix
  ;

parampattern
  : ('not' | 'addressof' | '-' | '-~' | MINUS_NEW | '-~' | 'digestof') parampattern
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

chain
  : '.>' ID
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
  | LPAREN_NEW rawseq tuple? ')'
  | LSQUARE_NEW ('as' type ':')? rawseq (',' rawseq)* ']'
  | 'object' ('\' ID (',' ID)* '\')? cap? ('is' type)? members 'end'
  | '{' ('\' ID (',' ID)* '\')? cap? ID? typeparams? ('(' | LPAREN_NEW) params? ')' lambdacaptures? (':' type)? '?'? '=>' rawseq '}' cap?
  | 'lambda' ('\' ID (',' ID)* '\')? cap? ID? typeparams? ('(' | LPAREN_NEW) params? ')' lambdacaptures? (':' type)? '?'? '=>' rawseq 'end'
  | '@' (ID | STRING) typeargs? ('(' | LPAREN_NEW) positional? named? ')' '?'?
  | '__loc'
  ;

atom
  : ID
  | 'this'
  | literal
  | ('(' | LPAREN_NEW) rawseq tuple? ')'
  | ('[' | LSQUARE_NEW) ('as' type ':')? rawseq (',' rawseq)* ']'
  | 'object' ('\' ID (',' ID)* '\')? cap? ('is' type)? members 'end'
  | '{' ('\' ID (',' ID)* '\')? cap? ID? typeparams? ('(' | LPAREN_NEW) params? ')' lambdacaptures? (':' type)? '?'? '=>' rawseq '}' cap?
  | 'lambda' ('\' ID (',' ID)* '\')? cap? ID? typeparams? ('(' | LPAREN_NEW) params? ')' lambdacaptures? (':' type)? '?'? '=>' rawseq 'end'
  | '@' (ID | STRING) typeargs? ('(' | LPAREN_NEW) positional? named? ')' '?'?
  | '__loc'
  ;

tuple
  : ',' rawseq (',' rawseq)*
  ;

lambdacaptures
  : ('(' | LPAREN_NEW) (lambdacapture | 'this') (',' (lambdacapture | 'this'))* ')'
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
  | ('(' | LPAREN_NEW) infixtype tupletype? ')'
  | nominal
  | lambdatype
  ;

lambdatype
  : '{' cap? ID? typeparams? ('(' | LPAREN_NEW) (type (',' type)*)? ')' (':' type)? '?'? '}' (cap | gencap)? ('^' | '!')?
  ;

tupletype
  : ',' infixtype (',' infixtype)*
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
  | '#alias'
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
  : '[' typearg (',' typearg)* ']'
  ;

typeparams
  : ('[' | LSQUARE_NEW) typeparam (',' typeparam)* ']'
  ;

params
  : (param | '...') (',' (param | '...'))*
  ;

typeparam
  : ID (':' type)? ('=' typearg)?
  ;

typearg
  : type
  | literal
  | '#' postfix
  ;

literal
  : 'true'
  | 'false'
  | INT
  | FLOAT
  | STRING
  ;

param
  : parampattern (':' type)? ('=' infix)?
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
  | chain
  | typeargs
  | call
  ;

antlr_3
  : dot
  | tilde
  | chain
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
  | '_' (LETTER | DIGIT | '_' | '\'')*
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

