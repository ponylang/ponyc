grammar ponyf;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | class_)*
  ;

use
  :  'use' STRING ('as' ID)?
  ;

class_
  :  'private'? ('actor' | 'class' | 'trait') ID type_params? mode_args? ('is' types)? '{' member* '}'
  ;

member
  :  'var' param
  |  'val' param
  |  'private'? ('def' | 'msg') ID type_params? mode_args? params type ('=' seq)?
  ;

typedef_
  :  'private'? 'type' ID ('.' ID)? type_params? (':' type)? ('is' types)? ('{' (ID '=' ID)* '}')?
  ;

types
  :  type (',' type)*
  ;

type
  :  base_type ('|' base_type)*
  ;

base_type
  :  '\\'? ID ('.' ID)? type_args? mode_args?
  |  params result mode_args?
  ;

type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' type_arg (',' type_arg)* ']'
  ;

type_arg
  :	 (type) => type
  |  expr
  ;

mode_args
  :	 '<' mode ('|' mode)* '>'
  ;

mode
  :	 'iso' | 'fro' | 'wri' | ID
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

param
  :  ID (':' type)? ('=' expr)?
  ;

result
  :  '(' type ')'
  ;

args
  :  '(' (arg (',' arg)*)? ')'
  ;

arg
  :  (ID '=') => ID '=' expr
  |  expr
  ;

seq
  :  expr (';' expr)*
  ;

expr
  :  'var' ID (':' type)? '=' expr
  |  'val' ID (':' type)? '=' expr
  |  binary ('=' expr)?
  |  'def' type_params? mode_args? params result '=' expr
  |  'if' seq 'then' expr 'else' expr
  |  'while' seq 'do' expr
  |  'for' ID 'in' seq 'do' expr // { var x = {e}.enumerator(); while x.has_next() do { var ID = x.next(); t } }
  |  'match' binary ('|' binary? ('as' ID ':' type)? ('if' binary)? '=' binary)+
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
  |  'reflect'
  |  'absorb' args
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
