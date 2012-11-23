grammar pony;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | declare | typedecl | trait | object | actor)*
  ;

use
  :  'use' (TYPEID '=')? STRING
  ;

declare
  :  'declare' typeclass is ('{' declaremap (',' declaremap)* '}')?
  ;

declaremap
  :  ID '=' ID
  ;

typedecl
  :  'type' TYPEID oftype is
  ;

oftype
  :  (':' typeelement ('|' typeelement)*)?
  ;

typeelement
  :	 '\\' typeclass
  |	 typeclass
  |  typelambda
  ;

typeclass
  :  TYPEID ('::' TYPEID)? annot? formalargs?
  ;

typelambda
  :  'lambda' annot? args ('->' args)? 'throws'?
  ;

trait
  :  'trait' TYPEID formalargs? is? '{' (constructor | function | message)* '}'
  ;

object
  :  'object' TYPEID formalargs? is? '{' (field | delegate | constructor | function)* '}'
  ;

actor
  :  'actor' TYPEID formalargs? is? '{' (field | delegate | constructor | function | message)* '}'
  ;

is
  :  'is' typeclass (',' typeclass)*
  ;

field
  :  'var' ID oftype ('=' expr)?
  ;

delegate
  :  'delegate' ID oftype
  ;

constructor
  :  ('new' | 'ambient') ID? formalargs? args 'throws'? block?
  ;

function
  :  'function' annot? ID? formalargs? args ('->' args)? 'throws'? block?
  ;

message
  :  'message' ID? formalargs? args block?
  ;

statement
  :  block
  |  conditional
  |  for_loop
  |  while_loop
  |  do_loop
//  |  do_loop ';'
  |  match
  |  command
//  |  command ';'
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

// FIX: could make the block optional for multiple cases that execute the same block
case_
  :  'case' case_varlist ('if' expr)? block
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

// FIX: this means an lvalue can't begin with a parenthesised expression or unop
// because foo( bar ) could be two expressions
lvalue
  :  'var' ID oftype
  |  atom (('.' ID formalargs?)? args)*
//  |  expr
  ;

expr_list
  :  expr (',' expr)*
  ;

expr
  :  unary (binop unary)*
  ;

unary
  :  unop unary
  |  typelambda ('is' block)?
  |  r_atom (('.' ID? formalargs?)? args)*
//  |  atom (('.' ID formalargs?)? args)*
  ;

r_atom
  :  atom
  |  '(' expr ')'
  |  '[' arglist ']'
  ;

atom
  :  ID
  |  typeclass
  |  'this'
  |  'true'
  |  'false'
  |  INT
  |  FLOAT
  |  STRING
//  |  '#[' arglist ']'
//  |  '[' arglist ']'
//  |  '(' expr ')'
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
  :  expr oftype ('=' expr)?
  ;

unop
  :  '-' | '!' | '\\'
  ;

// FIX: != and #= could be wrong
// var a:Type!=Type!(thing)
// var a:Type#=Type#(thing)
binop
  :  '+' | '-' | '*' | '/' | '%'
  |  '<<' | '>>'
  |  '<' | '<=' | '>=' | '>'
  |  '==' | '!=' | '#=' | '~='
  |  '|' | '&' | '^'
  ;

annot
  : '!' | '@' | '#' | '?'    
  ;

// Lexer

ID
  :  ('a'..'z' | '_') (LETTER | DIGIT | '_')*
  ;

TYPEID
  :  ('A'..'Z') (LETTER | DIGIT)*
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
