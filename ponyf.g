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
  :  'actor'? ('class' | 'trait') ID type_args ('is' types)? '{' member* '}'
  ;

member
  :  'var' arg
  |  'function' ID '(' args? ')' type ('=' expr)?
  ;

types
  :  type (',' type)*
  ;

type
  :  ID ('.' ID)? ('[' type_args? ']')?
  ;

type_args
  :  type_arg (',' type_arg)*
  ;

type_arg
  :  ID (':' type)?
  ;

args
  :  arg (',' arg)*
  ;

arg
  :  ID ':' type
  ;

expr_list
  :  expr (',' expr)*
  ;

expr
  :  top (';' top)*
  ;

top
  :  'var' ID '=' top
  |  'if' binary 'then' binary 'else' binary
  |  'match' binary ('case' binary? ('as' arg)? ('if' binary)? '=' binary)+
  |  'while' binary 'do' binary
  |  'for' ID 'in' binary 'do' binary // { var x = e1.enumerator(); while x.has_next() do { var ID = x.next(); e2 } }
  |  binary ('=' binary)?
  ;

binary
  :  unary (binop unary)*
  ;

unary
  :  unop unary
  |  call
  ;

call
  :  primary
  (  '.' primary
  |  '(' expr_list? ')'
  )*
  ;

primary
  :  'this'
  |  constant
  |  ID
  |  '{' expr '}'
  |  'reflect'
  |  'absorb' '(' expr ')'
  |  'type' '(' ID ',' expr ')'
  |  'field' '(' expr ',' expr ')'
  |  'method' '(' expr ',' expr ')'
  ;

constant
  :  'true'
  |  'false'
  |  INT
  |  FLOAT
  |  STRING
  ;

unop
  :  'not'
  |  '-'
  |  '\\'
  ;

binop
  :  'and'
  |  'or'
  ;

// Lexer

ID
  :  ('a'..'z' | 'A'..'Z' | '_') (LETTER | DIGIT | '_')*
  ;

INT
  :  ('0' | '1'..'9' DIGIT*)
  |  '0' 'x' HEX_DIGIT+
  |  '0' 'b' ('0' | '1' | '_')+
  ;

FLOAT
  :  DIGIT+ ('.' DIGIT+)? EXPONENT?
  ;

LINECOMMENT
  :  '//' ~('\n' | '\r')* '\r'? '\n' {$channel=HIDDEN;}
  ;

NESTEDCOMMENT
  :  '/*' ( ('/*') => NESTEDCOMMENT | ~'*' | '*' ~'/')* '*/' {$channel=HIDDEN;}
  ;

WS
  :  ( ' ' | '\t' | '\r' | '\n' ) {$channel=HIDDEN;}
  ;

STRING
  :  '"' ( ESC_SEQ | ~('\\'|'"') )* '"'
  ;

fragment
EXPONENT
  :  ('e' | 'E') ('+' | '-')? DIGIT+
  ;

fragment
LETTER
  :  ('a'..'z' | 'A'..'Z')
  ;

fragment
DIGIT
  :  '0'..'9'
  ;

fragment
HEX_DIGIT
  :  (DIGIT | 'a'..'f' | 'A'..'F' | '_')
  ;

fragment
ESC_SEQ
  :  '\\' ('a' | 'b' | 'f' | 'n' | 'r' | 't' | 'v' | '\"' | '\\' | '0')
  |  HEX_ESC
  |  UNICODE_ESC
  |  UNICODE2_ESC
  ;

fragment
HEX_ESC
  :  '\\' 'x' HEX_DIGIT HEX_DIGIT
  ;

fragment
UNICODE_ESC
  :  '\\' 'u' HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT
  ;

fragment
UNICODE2_ESC
  :  '\\' 'U' HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT
  ;
