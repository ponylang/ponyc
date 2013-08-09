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
  :  ('actor' | 'class' | 'trait') ID type_params? mode ('is' types)? protection member*
  ;

member
  :  'var' ID oftype
  |  'val' ID oftype
  |  ('fun' | 'msg') mode ID type_params? params oftype? protection ('=' expr)?
  ;

typedef_
  :  'type' ID ('.' ID)? type_params? oftype? ('is' types)? ('with' (ID '=' ID)+)?
  ;

protection
  :  ('private' | 'package')?
  ;

types
  :  type (',' type)*
  ;

type
  :  base_type
  |  '(' base_type (('|' | '&') type)* ')'
  ;

// FIX: a function that takes a single ADT looks weird
// fun{val}((I32|F32))
// only a problem when writing the type, not an issue for a lambda or a signature due to ID
// fun{val}(a:(I32|F32))
// could wrap ADT in different delimeter: <> or {} or [], but it would make intersection types weird
base_type
  :  '\\'? ID ('.' ID)? type_args? mode
  |  'fun' mode '(' types? ')' oftype?
  ;

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

mode
  :  ('{' base_mode ('|' base_mode)* '}' ('->' ('this' | ID))?)?
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
  :  'var' ID oftype? '=' expr
  |  'val' ID oftype? '=' expr
  |  binary ('=' expr)?
  |  'fun' mode params oftype? '=' expr
  |  'if' expr 'then' expr ('else' expr | 'end') // without else clause: if e1 then (e2; None) else None
  |  'match' expr ('|' binary? ('as' ID oftype)? ('if' binary)? '=' seq)+ 'end'
  |  'while' expr 'do' expr // value is None
  |  'do' expr 'while' expr // value is None
  |  'for' ID 'in' expr 'do' expr // (var x = (e1).iterator(); while x.has_next() do (var ID = x.next(); e2))
  |  'break' // only valid in a loop, exits the loop
  |  'continue' // only valid in a loop, short circuits the loop
  |  'return' expr // short circuits the function
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
  |  'swap' args
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
