grammar pony2;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | declare | typedef_ | class_)*
  ;

use
  :  'use' (ID '=')? STRING
  ;

declare
  :  'declare' expr is ('{' declaremap (',' declaremap)* '}')?
  ;

declaremap
  :  ID '=' ID
  ;

typedef_
  :  'type' ID oftype? is
  ;

class_
  :  'actor'? ('class' | 'trait') ID formalargs? is? '{' member* '}'
  ;

is
  :  'is' expr (',' expr)*
  ;

member
  :  'var' ID oftype? ofval?
  |  'val' ID oftype? ofval?
  |  'delegate' ID oftype
  |  'new' signature
  |  'ambient' signature
  |  'function' signature
  |  'message' signature
  ;

signature
  :	 mode? ID? formalargs? args ('->' args)? 'throws'? block?
  ;

oftype
  :  ':' expr
  ;

ofval
  :  '=' expr
  ;

statement
  :  block
  |  conditional
  |  for_loop
  |  while_loop
  |  do_loop ';'
  |  match
  |  command ';'
  ;

block
  :  '{' statement* catch_? always? '}'
  ;

conditional
  :  'if' expr block ('else' (block | conditional))?
  ;

for_loop
  :  'for' for_varlist 'in' expr block
  ;

for_varlist
  :  for_var (',' for_var)*
  ;

for_var
  :  ID oftype?
  ;

while_loop
  :  'while' expr block
  ;

do_loop
  :  'do' block 'while' expr
  ;

match
  :  'match' expr_list '{' case_+ '}'
  ;

case_
  :  'case' case_condlist? ('if' expr)? block?
  ;

case_condlist
  :  case_cond (',' case_cond)*
  ;

case_cond
  :  expr case_as?
  |  case_as
  ;

case_as
  :	  'as' ID oftype?
  ;

catch_
  :  'catch' block
  ;

always
  :  'always' block
  ;

command
  :  lvalue_list ('=' expr_list)?
  |  'return'
  |  'continue'
  |  'break'
  |  'throw'
  ;

lvalue_list
  :  lvalue (',' lvalue)*
  ;

lvalue
  :  'var' ID oftype?
  |  'val' ID oftype?
  |  expr
  ;

expr_list
  :  expr (',' expr)*
  ;

expr
  :  unary (binop unary)*
  ;

unary
  :  unop unary
  |  call
  ;

call
  :  atom
  (  mode
  |  formalargs
  |  args
  |  '.' ID
  )*
  ;

atom
  :  ID
  |  'this'
  |  'true'
  |  'false'
  |  INT
  |  FLOAT
  |  STRING
  |  '[' arglist ']'
  |  '(' expr ')'
  ;
  
// FIX: != could be wrong
// var a:Type!=Type!(thing)
mode
  :  '~' | '!' | '@' | '[:' expr ']'
  ;

formalargs
  :  '[' arglist ']'
  ;

args
  :  '(' arglist? ')'
  ;

arglist
  :  arg (',' arg)*
  ;

arg
  :  expr oftype? ofval?
  ;

unop
  :  '-' | '!' | '\\'
  ;

binop
  :  '+' | '-' | '*' | '/' | '%'
  |  '<<' | '>>'
  |  '<' | '<=' | '>=' | '>'
  |  '==' | '!='
  |  '|' | '&' | '^'
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
