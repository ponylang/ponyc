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
  :  ('actor' | 'class' | 'trait') ID type_params? mode protection ('is' types)? member*
  ;

member
  :  ('var' | 'val') ID oftype
  |  ('fun' | 'msg') mode ID type_params? params oftype? protection ('=' seq)?
  ;

typedef_
  :  'type' ID ('.' ID)* type_params? oftype? ('is' types)? ('with' (ID '=' ID)*)?
  ;

protection
  :  ('private' | 'package' | 'infer')?
  ;

types
  :  type (',' type)*
  ;

type
  :  base_type
  |  '(' base_type (('|' | '&') type)* ')'
  ;

base_type
  :  '\\'? ID ('.' ID)* type_args? mode
  |  'fun' mode '(' types? ')' oftype?
  ;

type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' type_arg (',' type_arg)* ']'
  ;

// FIX: '=' in front of expr type arg is unfortunate
type_arg
  :	 type
  |  '=' expr
  ;

mode
  :  ('{' (base_mode ('|' base_mode)*)? ('->' ('this' | ID))? '}')?
  ;

base_mode
  :	 'iso' | 'var' | 'val' | 'tag' | ID
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
  :  ('var' | 'val') ID oftype? '=' expr
     // return the value of binary, not expr. a = b returns a, b = a = b is a swap
  |  binary ('=' expr)?
     // expr is in a new scope
  |  'fun' mode params oftype? '=' expr
     // without else clause: if e1 then (e2; None) else None
     // whole thing is a scope, each clause is a subscope
  |  'if' seq 'then' expr ('else' expr | 'end')
     // whole thing is a scope, each seq is a scope
  |  'match' seq ('|' binary? ('as' ID oftype)? ('if' binary)? '=' seq)* 'end'
     // value is None
     // whole thing is a scope, expr is a subscope
  |  'while' seq 'do' expr
     // ((e1); (while e2 do (e1)))
  |  'do' expr 'while' expr
     // (var x = e1.iterator(); while x.has_next() do (var id = x.next(); e2))
  |  'for' ID oftype? 'in' expr 'do' expr
     // only valid in a while loop, exits the loop
  |  'break'
     // only valid in a while loop, short circuits the loop
  |  'continue'
     // short circuits the function
  |  'return' expr
  |  ';'
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
  |  '(' seq ')'
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
