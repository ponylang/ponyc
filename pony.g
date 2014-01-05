grammar pony;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | class_ | alias)*
  ;

use
  :  'use' STRING ('as' ID)?
  ;

class_
  :  ('actor' | 'class' | 'trait') ID type_params? ('is' types)? member*
  ;

member
  :  field | function | behaviour | constructor
  ;

field
  :  ('var' | 'val') ID oftype
  ;

function
  :  'fun' mode '?'? ID type_params? params oftype? ('=>' seq)?
  ;

behaviour
  :  'be' ID type_params? params ('=>' seq)?
  ;
 
constructor
  :  'new' mode '?'? ID type_params? params ('=>' seq)?
  ;

alias
  :  'alias' ID type_params? oftype
  ;

types
  :  type (',' type)*
  ;

type
  :  base_type
  |  '(' base_type ('|' base_type)* ')'
  ;

base_type
  :  ID ('.' ID)? type_args? mode
  |  'fun' mode '?'? '(' types? ')' oftype?
  ;

type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' type (',' type)* ']'
  ;

mode
  :  ('{' base_mode '}')? ('->' ('this' | ID))? ('*')?
  ;

base_mode
  :  'iso' | 'trn' | 'var' | 'val' | 'box' | 'tag' | 'this' | ID
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

param
  :  ID oftype? ('=' expr)?
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

expr
  :  ('var' | 'val') ID oftype? '=' expr
  |  binary ('=' expr)?
  |  'fun' mode '?'? params oftype? '=>' expr
  |  'if' seq 'then' seq ('else' expr | 'end')
  |  'match' seq case* 'end'
  |  'while' seq 'do' expr
  |  'do' seq 'while' expr
  |  'for' ID oftype? 'in' seq 'do' expr
  |  'break'
  |  'continue'
  |  'return'
  |  'try' seq ('else' seq)? ('then' seq)? 'end'
  |  'undef'
  ;

case
  :  '|' binary? ('as' ID oftype)? ('if' binary)? ('=>' seq)?
  ;

seq
  :  expr (';' expr)*
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
  |  INT
  |  FLOAT
  |  STRING
  |  ID
  |  '(' seq ')'
//  |  '{' seq '}' // FIX: can use this for object literals? comprehensions?
  ;

unop
  :  'not' | '-'
  ;

binop
  :  'and' | 'or' | 'xor'
  |  '+' | '-' | '*' | '/' | '%'
  |  '<<' | '>>'
  |  '==' | '!=' | '<' | '<=' | '>=' | '>'
  ;

// Lexer

ID
  :  (LETTER | '_') (LETTER | DIGIT | '_' | '\'')*
  ;

INT
  :  DIGIT+
  |  '0' 'x' HEX+
  |  '0' 'b' ('0' | '1')+
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
