grammar pony2;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | class_ | typedecl)*
  ;

use
  :  'use' STRING ('as' ID)?
  ;

class_
  :  ('actor' | 'class' | 'trait') ID type_params? ('is' types)? member*
  ;

member
  :  ('var' | 'val') ID oftype? (':=' seq)? // field
  |  'new' '?'? ID type_params? params body? // constructor
  |  'fun' mode '?'? ID type_params? params oftype? body? // function
  |  'be' ID type_params? params body? // behaviour
  ;

typedecl
  :  'type' ID type_params? (':' (base_type | type_expr))?
  ;
  
oftype
  :  ':' type
  ;

types
  :  type (',' type)*
  ;

type
  :  (base_type | type_expr) '^'? // ephemeral types
  ;

type_expr
  :  '(' base_type (typeop base_type)* ')' mode? // ADT or tuple
  ;

typeop
  :  '|' | '&' | ',' // union, intersection, tuple
  ;

base_type
  :  ID ('.' ID)* type_args? mode? // nominal type
  |  '{' fun_type* '}' mode? // structural type
  |  typedecl // nested type definition
  ;

fun_type
  :  'fun' mode '?'? ID? '(' types? ')' oftype?
  |  'be' ID? '(' types? ')' // FIX: need this?
  ;

type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' type (',' type)* ']'
  ;

mode
  :  ':' (base_mode | ID)
  ;

base_mode
  :  ('iso' | 'trn' | 'var' | 'val' | 'box' | 'tag')
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

param
  :  ID oftype? (':=' seq)?
  ;

args
  :  '(' (seq (',' seq)*)? optargs? ')'
  ;

optargs
  :  'with' ID ':=' seq (',' ID ':=' seq)*
  ;

body
  :  '->' seq
  ;

seq
  :  expr (';' expr)*
  ;

expr
  :  binary
  |  'return' binary
  |  'break' binary
  |  'continue'
  |  'undef'
  ;

binary
  :  term (binop term)*
  ;

term
  :  local
  |  control
  |  postfix
  |  unop term
  |  'consume' ID ('.' ID)*
  |  'recover' term
  ;

local
  :  ('var' | 'val') (ID | '(' ID (',' ID)* ')') oftype?
  ;

control
  :  'if' seq 'then' seq ('elseif' seq 'then' seq)* end
  |  'match' seq? case* end
  |  'while' seq 'do' seq end
  |  'do' seq 'while' seq 'end'
  |  'for' ID oftype? 'in' seq 'do' seq end
  |  'try' seq end
  ;

end
  :  ('else' seq)? 'end'
  ;

case
  :  '|' expr? ('as' ID oftype)? ('when' seq)? body?
  ;

postfix
  :  atom
  (  '.' (ID | INT) // member or tuple component
  |  type_args // type arguments
  |  args // method arguments
  )*
  ;

atom
  :  INT
  |  FLOAT
  |  STRING
  |  ID
  |  '(' seq (',' seq)* ')' // tuple constructor
  |  '[' seq (',' seq)* ']' // array literal
  |  '{' ('is' types)? member* '}' // object literal
  ;

unop
  :  'not' | '-'
  ;

binop
  :  'and' | 'or' | 'xor' // logic
  |  '+' | '-' | '*' | '/' | '%' // arithmetic
  |  '<<' | '>>' // shift
  |  'is' | '==' | '!=' | '<' | '<=' | '>=' | '>' // equality
  |  ':=' // assignment
  |  '=>' // shorthand for (left, right) tuple FIX: not needed
  ;

// Lexer

ID
  :  (LETTER | '_') (LETTER | DIGIT | '_' | '\'')*
  ;

INT
  :  DIGIT+
  |  '0' 'x' HEX+
  |  '0' 'o' OCTAL+
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
OCTAL
  :  '0'..'7'
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
