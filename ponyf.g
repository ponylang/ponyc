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
  :  'use' (ID '=')? STRING
  ;

class_
  :  ('actor' | 'class' | 'trait') ID type_params? ('is' types)? '{' member* '}'
  ;

member
  :  'var' param
  |  'val' param
  |  'private'? ('def' | 'msg') ID type_params? params type ('=' expr)?
  ;

typedef_
  :  'type' ID ('.' ID)? type_params? (':' type)? ('is' types)? ('{' (ID '=' ID)* '}')?
  ;

types
  :  type (',' type)*
  ;

type
  :  base_type ('|' base_type)*
  ;

base_type
  :  '\\'? ID ('.' ID)? type_args?
  |  params result
  ;

type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' arg (',' arg)* ']'
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

param
  :  ID (':' type)? ('=' term)?
  ;

result
  :  '(' type ')'
  ;

args
  :  '(' (arg (',' arg)*)? ')'
  ;

arg
  :  (ID '=') => ID '=' term
  |  term
  ;

expr
  :  term (';' term)*
  ;

term
  :  'var' ID (':' type)? '=' term
  |  'val' ID (':' type)? '=' term
  |  binary ('=' term)?
  |  'def' type_params? params result '=' term
  |  'if' expr 'then' term 'else' term
  |  'while' expr 'do' term
  |  'for' ID 'in' expr 'do' term // { var x = {e}.enumerator(); while x.has_next() do { var ID = x.next(); t } }
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
  (  '.' primary
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
  |  '{' expr '}'
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
  |  '==' | '<>' | '<' | '<=' | '>=' | '>'
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
