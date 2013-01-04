grammar pony2;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | declare | typedecl | class_)*
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

typedecl
  :  'type' ID oftype is
  ;

oftype
  :  (':' expr)?
  ;

ofval
  :  ('=' expr)?
  ;

class_
  :  'actor'? ('class' | 'trait') ID formalargs? is? '{' (field | delegate | constructor | function | message)* '}'
  ;

is
  :  'is' expr (',' expr)*
  ;

field
  :  'var' ID oftype ofval
  |  'val' ID oftype ofval
  ;

delegate
  :  'delegate' ID oftype
  ;

constructor
  :  ('new' | 'ambient') mode? ID? formalargs? args 'throws'? block?
  ;

function
  :  'function' mode? ID? formalargs? args ('->' args)? 'throws'? block?
  ;

message
  :  'message' mode? ID? formalargs? args block?
  ;

statement
  :  block
  |  conditional
  |  for_loop
  |  while_loop
  |  do_loop ';'
  |  match
  |  command ';'
  |  'return'
  |  'break'
  |  'continue'
  |  'throw'
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
  :  ID oftype
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
  :  'case' case_varlist ('if' expr)? block?
  ;

case_varlist
  :  (case_var (',' case_var)*)?
  ;

case_var
  :  expr ('as' ID oftype)?
  |  'as' ID oftype
  ;

catch_
  :  'catch' block
  ;

always
  :  'always' block
  ;

command
  :  lvalue_list ('=' expr_list)?
  ;

lvalue_list
  :  lvalue (',' lvalue)*
  ;

lvalue
  :  'var' ID oftype
  |  'val' ID oftype
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
  |  formalargs args
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

// the first expr is restricted to ID or ID:type if the assignment is present
// the :type is allowed only if the first expr is ID
arg
  :  expr oftype ofval
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
