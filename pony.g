grammar pony;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  use* (typealias | class_)*
  ;

use
  :  'use' (ID '=')? (STRING | use_ffi) ('where' expr)?
  ;

use_ffi
  :  '@' ID type_args '(' params? ')' '?'?
  ;

typealias
  :  'type' ID 'is' type
  ;

class_
  :  ('interface' | 'trait' | 'primitive' | 'class' | 'actor') ID type_params? cap? ('is' types)? members
  ;

members
  :  field* method*
  ;

field
  :  ('var' | 'let') ID oftype? ('=' infix)?
  ;

method
  :  ('fun' | 'be' | 'new') cap? ID? type_params? '(' params? ')' oftype? '?'? '=>'? seq?
  ;

oftype
  :  ':' type
  ;

types
  :  type (',' type)*
  ;

type
  :  atom_type ('->' type)?
  ;

atom_type
  :  'this' // only used for viewpoint adaptation
  |  '(' tuple_type ')'
  |  ID ('.' ID)? type_args? cap? ('^' | '!')? // nominal type
  ;

tuple_type
  :  infix_type (',' infix_type)*
  ;

infix_type
  :  type (('|' | '&') type)*
  ;

// the @ is a cheat: means the symbol "not on a new line"
// without the @, it could be on a new line or not
type_params
  :  '@[' type_param (',' type_param)* ']'
  ;

type_param
  :  ID oftype? ('=' type)?
  ;

type_args
  :  '@[' type (',' type)* ']'
  ;

cap
  :  'iso' | 'trn' | 'ref' | 'val' | 'box' | 'tag'
  ;

params
  :  param (',' param)*
  ;

param
  :  ID oftype ('=' infix)?
  | '...'
  ;

seq
  :  (expr)+
  ;

expr
  :  (jump | assignment) ';'?
  ;
  
jump
  :  'return' assignment
  |  'break' assignment
  |  'continue'
  |  'error'
  ;

assignment
  :  infix ('=' assignment)?
  ;

infix
  :  term (binop term)*
  ;

term
  :  local
  |  control
  |  postfix
  |  unop term
  ;

local
  :  ('var' | 'let') idseq oftype?
  ;

control
  :  'if' seq 'then' seq (elseif | ('else' seq))? 'end'
  |  'match' seq case* ('else' seq)? 'end'
  |  'while' seq 'do' seq ('else' seq)? 'end'
  |  'repeat' seq 'until' seq ('else' seq)? 'end'
  |  'for' idseq oftype? 'in' seq 'do' seq ('else' seq)? 'end'
  |  'try' seq ('else' seq)? ('then' seq)? 'end'
  |  'with' withexpr (',' withexpr)* 'do' seq ('else' seq)? 'end'
  |  'recover' seq 'end'
  |  'use' ':'? '(' (expr)+ ')' // For testing only
  ;

elseif
  :  'elseif' seq 'then' seq (elseif | ('else' seq))?
  ;

case
  :  '|' infix? ('where' seq)? ('=>' seq)?
  ;

withexpr
  :  idseq oftype '=' seq
  ;

postfix
  :  atom
     (  '.' ID // member access
     |  '!' ID // partial application, syntactic sugar
     |  type_args // type arguments
     |  '@(' positional? named? ')' // call
     )*
  ;

atom
  :  INT
  |  FLOAT
  |  STRING
  |  ID
  |  'this'
  |  '(' positional ')' // tuple
  |  '[' positional? named? ']' // array
  |  '{' ('is' types)? members '}' // object
  |  '@' ID ('.' ID)* type_args? '(' positional? ')' '?'? // ffi
  ;

idseq
  :  ID | idseq_multi
  ;

idseq_multi
  :  '(' idseq_element (',' idseq_element)* ')'
  ;

idseq_element
  :  ID
  |  idseq_multi
  ;

positional
  :  seq (',' seq)*
  ;

named
  :  'where' ID '=' seq (',' ID '=' seq)*
  ;

unop
  :  'not' | '-' | 'consume' | '&'
  ;

binop
  :  'and' | 'or' | 'xor' // logic
  |  '+' | '@-' | '*' | '/' | '%' // arithmetic
  |  '<<' | '>>' // shift
  |  'is' | 'isnt' | '==' | '!=' | '<' | '<=' | '>=' | '>' // comparison
  ;

/* Precedence

Value:
1. postfix
2. unop
3. binop
4. =
5. seq
6. ,

Type:
1. ->
2. & |
3. ,
*/

// Lexer

ID
  :  (LETTER | '_') (LETTER | DIGIT | '_' | '\'')*
  ;

INT
  :  DIGIT+
  |  '0' 'x' HEX+
  |  '0' 'b' BINARY+
  |  '\'' (ESC | ~('\'' | '\\'))* '\''
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
  |  '"""' ~('"""')* '"""'
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
BINARY
  :  '0'..'1'
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
  :  '\\' ('a' | 'b' | 'e' | 'f' | 'n' | 'r' | 't' | 'v' | '\"' | '\\' | '0')
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
