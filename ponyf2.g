grammar ponyf2;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | class_ | typedef_)*
  ;

use
  :  'use' STRING ('as' ID)?
  ;

class_
  :  ('actor' | 'class' | 'trait') mode? ID type_params? ('is' types)? member*
  ;

member
  :  'var' ID oftype
  |  'val' ID oftype
  |  ('fun' | 'msg') ID type_params? params oftype? ('=' expr)?
  ;

typedef_
  :  'type' ID ('.' ID)? type_params? oftype? ('is' types)? ('with' (ID '=' ID)+)?
  ;

// FIX: add to types and functions
protection
  :  ('private' | 'package')?
  ;

types
  :  type (',' type)*
  ;

type
  :  base_type
  |  '(' base_type (('|' | '&') base_type)* ')'
  ;

base_type
  :  '\\'? ID ('.' ID)? type_args?
  |  'fun' '(' types ')' oftype?
  ;

/* FIX: accept a mode as a type param? */
type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' type_arg (',' type_arg)* ']'
  ;

type_arg
  :	 type
  |  'val' expr
  ;

// FIX: reference annotation
mode_mod
  :  (mode | ID) ('->' ('this' | ID))?
  ;

mode
  :	 'isolated' | 'freezable' | 'frozen' | 'write' | 'read' | 'opaque'
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

param
  :  ID (oftype ('=' expr)? | '=' expr)
  ;

oftype
  :  ':' type
  ;

args
  :  '(' (arg (',' arg)*)? ')'
  ;

arg
  :  expr ('->' ID)?
  ;

seq
  :  expr (';' expr)*
  ;

expr
  :  'var' ID oftype? '=' expr
  |  'val' ID oftype? '=' expr
  |  binary ('=' expr)?
  |  'fun' mode_mod? params oftype? '=' expr
  |  'if' expr 'then' expr ('else' expr | 'end') // without else clause: if e1 then {e2; None} else None
  |  'match' expr ('|' binary? ('as' ID oftype)? ('if' binary)? '=' seq)+ 'end'
  |  'while' expr 'do' expr // value is None
  |  'do' expr 'while' expr // value is None
  |  'for' ID 'in' expr 'do' expr // { var x = {e1}.iterator(); while x.has_next() do { var ID = x.next(); e2 } }, value is None
  |  'break' // only valid in a loop, exits the loop
  |  'continue' // only valid in a loop, short circuits the loop
  |  'return' expr // short circuits the function
  ;

binary
  :  unary (binop unary)*
  ;

unary
  :  unop unary
  |  postfix
  ;

postfix
  :  primary
  (  '.' ID
  |  type_args
  |  args
  )*
  ;

primary
  :  'this'
  |  'true'
  |  'false'
  |  INT
  |  FLOAT
  |  STRING
  |  ID
  |  '{' seq '}'
  |  'field' args
  |  'method' args
  ;

unop
  :  'not' | '-' | '\\'
  ;

binop
  :  'and' | 'or' | 'xor'
  |  '+' | '-' | '*' | '/' | '%'
  |  '<<' | '>>'
  |  '==' | '!=' | '<' | '<=' | '>=' | '>'
  ;

// Lexer

ID
  :  (LETTER | '_') (LETTER | DIGIT | '_')*
  ;

INT
  :  '0'
  |  '1'..'9' DIGIT*
  |  '0' 'x' (HEX | '_') +
  |  '0' 'b' ('0' | '1' | '_')+
  ;

FLOAT
  :  DIGIT+ ('.' DIGIT+)? EXP?
  ;

LINECOMMENT
  :  '//' ~('\n' | '\r')* '\r'? '\n' {$channel=HIDDEN;}
  ;

NESTEDCOMMENT
  :  '/*' ( ('/*') => NESTEDCOMMENT | ~'*' | '*' ~'/')* '*/'
  ;

WS
  :  ' ' | '\t' | '\r' | '\n'
  ;

STRING
  :  '"' ( ESC | ~('\\'|'"') )* '"'
  ;

fragment
EXP
  :  ('e' | 'E') ('+' | '-')? DIGIT+
  ;

fragment
LETTER
  :  'a'..'z' | 'A'..'Z'
  ;

fragment
DIGIT
  :  '0'..'9'
  ;

fragment
HEX
  :  DIGIT | 'a'..'f' | 'A'..'F'
  ;

fragment
ESC
  :  '\\' ('a' | 'b' | 'f' | 'n' | 'r' | 't' | 'v' | '\"' | '\\' | '0')
  |  HEX_ESC
  |  UNICODE_ESC
  |  UNICODE2_ESC
  ;

fragment
HEX_ESC
  :  '\\' 'x' HEX HEX
  ;

fragment
UNICODE_ESC
  :  '\\' 'u' HEX HEX HEX HEX
  ;

fragment
UNICODE2_ESC
  :  '\\' 'U' HEX HEX HEX HEX HEX HEX
  ;
