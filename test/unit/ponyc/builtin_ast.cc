const char* t_bool =
  "(uniontype"
  "  (nominal{dataref True} x (id True) x tag x)"
  "  (nominal{dataref False} x (id False) x tag x))";

const char* t_u_lit =
  "(nominal{dataref UIntLiteral} x (id UIntLiteral) x tag x)";

const char* t_s_lit =
  "(nominal{dataref SIntLiteral} x (id SIntLiteral) x tag x)";

const char* t_f_lit =
  "(nominal{dataref FloatLiteral} x (id FloatLiteral) x tag x)";

const char* t_none =   "(nominal{dataref None} x (id None) x tag x)";
const char* t_true =   "(nominal{dataref True} x (id True) x tag x)";
const char* t_false =  "(nominal{dataref False} x (id False) x tag x)";
const char* t_u8 =     "(nominal{dataref U8} x (id U8) x tag x)";
const char* t_u16 =    "(nominal{dataref U16} x (id U16) x tag x)";
const char* t_u32 =    "(nominal{dataref U32} x (id U32) x tag x)";
const char* t_u64 =    "(nominal{dataref U64} x (id U64) x tag x)";
const char* t_u128 =   "(nominal{dataref U128} x (id U128) x tag x)";
const char* t_i8 =     "(nominal{dataref I8} x (id I8) x tag x)";
const char* t_i16 =    "(nominal{dataref I16} x (id I16) x tag x)";
const char* t_i32 =    "(nominal{dataref I32} x (id I32) x tag x)";
const char* t_i64 =    "(nominal{dataref I64} x (id I64) x tag x)";
const char* t_i128 =   "(nominal{dataref I128} x (id I128) x tag x)";
const char* t_f32 =    "(nominal{dataref F32} x (id F32) x tag x)";
const char* t_f64 =    "(nominal{dataref F64} x (id F64) x tag x)";
const char* t_string = "(nominal{dataref String} x (id String) x val x)";
const char* t_t1 =     "(nominal{dataref T1} x (id T1) x val x)";
const char* t_t2 =     "(nominal{dataref T2} x (id T2) x val x)";
const char* t_foo =    "(nominal{dataref Foo} x (id Foo) x val x)";
const char* t_bar =    "(nominal{dataref Bar} x (id Bar) x val x)";

const char* t_structural =
  "(structural"
  "  (members"
  "    (fun{scope} box (id f) x x"
  "      (nominal{dataref None} x (id None) x tag x) x x)) val x)";


const char* builtin =
"(package"
"  {scope None True False Bool I8 I16 I32 I64 I128 U8 U16 U32 U64 U128"
"    F32 F64 SIntLiteral UIntLiteral FloatLiteral String"
"    SInt UInt Integer Float Arithmetic}"
"  (module{scope}{def test}"

// Builtin definitions
"    (primitive{def None} (id None) x tag x x)"
"    (primitive{def True} (id True) x tag x x)"
"    (primitive{def False} (id False) x tag x x)"
"    (type{def Bool} (id Bool)"
"      (uniontype{def BoolUse}"
"        (nominal{dataref True} x (id True) x tag x)"
"        (nominal{dataref False} x (id False) x tag x)))"
"    (primitive{def I8} (id I8) x tag x x)"
"    (primitive{def I16} (id I16) x tag x x)"
"    (primitive{def I32} (id I32) x tag x x)"
"    (primitive{def I64} (id I64) x tag x x)"
"    (primitive{def I128} (id I128) x tag x x)"
"    (primitive{def U8} (id U8) x tag x x)"
"    (primitive{def U16} (id U16) x tag x x)"
"    (primitive{def U32} (id U32) x tag x x)"
"    (primitive{def U64} (id U64) x tag x x)"
"    (primitive{def U128} (id U128) x tag x x)"
"    (primitive{def F32} (id F32) x tag x x)"
"    (primitive{def F64} (id F64) x tag x x)"
"    (primitive{def SIntLiteral} (id SIntLiteral) x tag x x)"
"    (primitive{def UIntLiteral} (id UIntLiteral) x tag x x)"
"    (primitive{def FloatLiteral} (id FloatLiteral) x tag x x)"
"    (class{def String} (id String) x val x x)"
"    (type{def SInt} (id SInt)"
"      (uniontype"
"        (nominal x (id SIntLiteral) x tag x)"
"        (nominal x (id I8) x tag x)"
"        (nominal x (id I16) x tag x)"
"        (nominal x (id I32) x tag x)"
"        (nominal x (id I64) x tag x)"
"        (nominal x (id I128) x tag x)))"
"    (type{def UInt} (id UInt)"
"      (uniontype"
"        (nominal x (id UIntLiteral) x tag x)"
"        (nominal x (id U8) x tag x)"
"        (nominal x (id U16) x tag x)"
"        (nominal x (id U32) x tag x)"
"        (nominal x (id U64) x tag x)"
"        (nominal x (id U128) x tag x)))"
"    (type{def Integer} (id Integer)"
"      (uniontype"
"        (nominal x (id SInt) x x x)"
"        (nominal x (id UInt) x x x)))"
"    (type{def Float} (id Float)"
"      (uniontype"
"        (nominal x (id FloatLiteral) x tag x)"
"        (nominal x (id F32) x tag x)"
"        (nominal x (id F64) x tag x)))"
"    (type{def Arithmetic} (id Arithmetic)"
"      (uniontype"
"        (nominal x (id Integer) x x x)"
"        (nominal x (id Float) x x x)))"

// User type definitions
"    (trait{scope}{def T1} (id T1) x ref x members)"
"    (trait{scope}{def T2} (id T2) x ref x members)"
"    (class{scope}{def Foo} (id Foo) x ref x members)"
"    (class{scope f}{def Bar} (id Bar) x ref"
"      (types (nominal{dataref T1} x (id T1) x ref x))"
"      (members"
"        (fun{scope}{def f} box (id f) x x"
"          (nominal{dataref None} x (id None) x tag x) x"
"          (seq (reference (id None))))))"
"))";
