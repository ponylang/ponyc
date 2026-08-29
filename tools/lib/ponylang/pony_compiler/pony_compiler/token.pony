use @token_new[NullablePointer[_Token]](id: TokenId)
use @token_free[None](token: _Token)
use @token_dup[_Token](token: _Token)
use @token_dup_new_id[_Token](token: _Token, id: TokenId)
use @token_get_id[TokenId](token: _Token)
use @token_string[Pointer[U8] ref](token: _Token)
use @token_string_len[USize](token: _Token)
use @token_float[F64](token: _Token)
use @token_int[NullablePointer[_LexIntT]](token: _Token)
// return a string for ptinting the given token.
// The returned string must not be deleted and is valid for the lifetime of the
// token
use @token_source[NullablePointer[_Source]](token: _Token)
use @token_print[Pointer[U8] ref](token: _Token)
use @token_id_desc[Pointer[U8] ref](token: _Token)
use @token_line_number[USize](token: _Token)
use @token_line_position[USize](token: _Token)

struct _Token
  var id: TokenId = TokenIds.tk_eof()
  var source: NullablePointer[_Source] = source.none()
  var line: USize = 0
  var pos: USize = 0
  var printed: Pointer[U8] ref = printed.create()
  var opaque_value_1: U64 = 0
  var opaque_value_2: U64 = 0
    """
    Probably wrong derive from, just to get the right struct
    layout for reaching the frozen field
    ```c
    union
    {
      struct
      {
        const char* string;
        size_t str_length;
      };
      double real;
      lexint_t integer;
    }
    ```
    """
  var frozen: Bool = false

type TokenId is I32

primitive TokenIds
  """
  Numeric values based upon the order of elements in the
  `token_id` enum in libponyc/ast/token.h
  """
  fun tk_eof(): I32 => 0
  fun tk_lex_error(): I32 => 1
  fun tk_none(): I32 => 2

  // Literals
  fun tk_true(): I32 => 3
  fun tk_false(): I32 => 4
  fun tk_string(): I32 => 5
  fun tk_int(): I32 => 6
  fun tk_float(): I32 => 7
  fun tk_id(): I32 => 8

  // Symbols
  fun tk_lbrace(): I32 => 9
  fun tk_rbrace(): I32 => 10
  fun tk_lparen(): I32 => 11
  fun tk_rparen(): I32 => 12
  fun tk_lsquare(): I32 => 13
  fun tk_rsquare(): I32 => 14
  fun tk_backslash(): I32 => 15

  fun tk_comma(): I32 => 16
  fun tk_arrow(): I32 => 17
  fun tk_dblarrow(): I32 => 18
  fun tk_dot(): I32 => 19
  fun tk_tilde(): I32 => 20
  fun tk_chain(): I32 => 21
  fun tk_colon(): I32 => 22
  fun tk_semi(): I32 => 23
  fun tk_assign(): I32 => 24

  fun tk_plus(): I32 => 25
  fun tk_plus_tilde(): I32 => 26
  fun tk_minus(): I32 => 27
  fun tk_minus_tilde(): I32 => 28
  fun tk_multiply(): I32 => 29
  fun tk_multiply_tilde(): I32 => 30
  fun tk_divide(): I32 => 31
  fun tk_divide_tilde(): I32 => 32
  fun tk_rem(): I32 => 33
  fun tk_rem_tilde(): I32 => 34
  fun tk_mod(): I32 => 35
  fun tk_mod_tilde(): I32 => 36
  fun tk_at(): I32 => 37
  fun tk_at_lbrace(): I32 => 38

  fun tk_lshift(): I32 => 39
  fun tk_lshift_tilde(): I32 => 40
  fun tk_rshift(): I32 => 41
  fun tk_rshift_tilde(): I32 => 42

  fun tk_lt(): I32 => 43
  fun tk_lt_tilde(): I32 => 44
  fun tk_le(): I32 => 45
  fun tk_le_tilde(): I32 => 46
  fun tk_ge(): I32 => 47
  fun tk_ge_tilde(): I32 => 48
  fun tk_gt(): I32 => 49
  fun tk_gt_tilde(): I32 => 50

  fun tk_eq(): I32 => 51
  fun tk_eq_tilde(): I32 => 52
  fun tk_ne(): I32 => 53
  fun tk_ne_tilde(): I32 => 54

  fun tk_pipe(): I32 => 55
  fun tk_isecttype(): I32 => 56
  fun tk_ephemeral(): I32 => 57
  fun tk_aliased(): I32 => 58
  fun tk_subtype(): I32 => 59

  fun tk_question(): I32 => 60
  fun tk_unary_minus(): I32 => 61
  fun tk_unary_minus_tilde(): I32 => 62
  fun tk_ellipsis(): I32 => 63
  fun tk_constant(): I32 => 64

  // Newline symbols only used by lexer and parser
  fun tk_lparen_new(): I32 => 65
  fun tk_lsquare_new(): I32 => 66
  fun tk_minus_new(): I32 => 67
  fun tk_minus_tilde_new(): I32 => 68

  // Keywords
  fun tk_compile_intrinsic(): I32 => 69

  fun tk_use(): I32 => 70
  fun tk_export(): I32 => 71
  fun tk_type(): I32 => 72
  fun tk_interface(): I32 => 73
  fun tk_trait(): I32 => 74
  fun tk_primitive(): I32 => 75
  fun tk_struct(): I32 => 76
  fun tk_class(): I32 => 77
  fun tk_actor(): I32 => 78
  fun tk_object(): I32 => 79
  fun tk_lambda(): I32 => 80
  fun tk_barelambda(): I32 => 81

  fun tk_as(): I32 => 82
  fun tk_is(): I32 => 83
  fun tk_isnt(): I32 => 84

  fun tk_var(): I32 => 85
  fun tk_let(): I32 => 86
  fun tk_embed(): I32 => 87
  fun tk_dontcare(): I32 => 88
  fun tk_new(): I32 => 89
  fun tk_fun(): I32 => 90
  fun tk_be(): I32 => 91

  fun tk_iso(): I32 => 92
  fun tk_trn(): I32 => 93
  fun tk_ref(): I32 => 94
  fun tk_val(): I32 => 95
  fun tk_box(): I32 => 96
  fun tk_tag(): I32 => 97

  fun tk_cap_read(): I32 => 98
  fun tk_cap_send(): I32 => 99
  fun tk_cap_share(): I32 => 100
  fun tk_cap_alias(): I32 => 101
  fun tk_cap_any(): I32 => 102

  fun tk_this(): I32 => 103
  fun tk_return(): I32 => 104
  fun tk_break(): I32 => 105
  fun tk_continue(): I32 => 106
  fun tk_consume(): I32 => 107
  fun tk_recover(): I32 => 108

  fun tk_if(): I32 => 109
  fun tk_ifdef(): I32 => 110
  fun tk_iftype(): I32 => 111
  fun tk_iftype_set(): I32 => 112
  fun tk_then(): I32 => 113
  fun tk_else(): I32 => 114
  fun tk_elseif(): I32 => 115
  fun tk_end(): I32 => 116
  fun tk_while(): I32 => 117
  fun tk_do(): I32 => 118
  fun tk_repeat(): I32 => 119
  fun tk_until(): I32 => 120
  fun tk_for(): I32 => 121
  fun tk_in(): I32 => 122
  fun tk_match(): I32 => 123
  fun tk_where(): I32 => 124
  fun tk_try(): I32 => 125
  fun tk_try_no_check(): I32 => 126
  fun tk_with(): I32 => 127
  fun tk_error(): I32 => 128
  fun tk_compile_error(): I32 => 129

  fun tk_not(): I32 => 130
  fun tk_and(): I32 => 131
  fun tk_or(): I32 => 132
  fun tk_xor(): I32 => 133

  fun tk_digestof(): I32 => 134
  fun tk_address(): I32 => 135
  fun tk_location(): I32 => 136

  // Abstract tokens which don't directly appear in the source
  fun tk_program(): I32 => 137
  fun tk_package(): I32 => 138
  fun tk_module(): I32 => 139

  fun tk_members(): I32 => 140
  fun tk_fvar(): I32 => 141
  fun tk_flet(): I32 => 142
  fun tk_ffidecl(): I32 => 143
  fun tk_fficall(): I32 => 144

  fun tk_ifdefand(): I32 => 145
  fun tk_ifdefor(): I32 => 146
  fun tk_ifdefnot(): I32 => 147
  fun tk_ifdefflag(): I32 => 148

  fun tk_provides(): I32 => 149
  fun tk_uniontype(): I32 => 150
  fun tk_tupletype(): I32 => 151
  fun tk_nominal(): I32 => 152
  fun tk_thistype(): I32 => 153
  fun tk_funtype(): I32 => 154
  fun tk_lambdatype(): I32 => 155
  fun tk_barelambdatype(): I32 => 156
  fun tk_dontcaretype(): I32 => 157
  fun tk_bool_true(): I32 => 158
  fun tk_bool_false(): I32 => 159
  fun tk_infertype(): I32 => 160
  fun tk_errortype(): I32 => 161

  // A literal expression whose type is not yet inferred
  fun tk_literal(): I32 => 162
  fun tk_literalbranch(): I32 => 163 // Literal branch of a control structure
  fun tk_operatorliteral(): I32 => 164 // Operator function access to a literal

  fun tk_typeparams(): I32 => 165
  fun tk_typeparam(): I32 => 166
  fun tk_params(): I32 => 167
  fun tk_param(): I32 => 168
  fun tk_typeargs(): I32 => 169
  fun tk_valueformalparam(): I32 => 170
  fun tk_valueformalarg(): I32 => 171
  fun tk_positionalargs(): I32 => 172
  fun tk_namedargs(): I32 => 173
  fun tk_namedarg(): I32 => 174
  fun tk_updatearg(): I32 => 175
  fun tk_lambdacaptures(): I32 => 176
  fun tk_lambdacapture(): I32 => 177

  fun tk_seq(): I32 => 178
  fun tk_qualify(): I32 => 179
  fun tk_call(): I32 => 180
  fun tk_tuple(): I32 => 181
  fun tk_array(): I32 => 182
  fun tk_cases(): I32 => 183
  fun tk_case(): I32 => 184
  fun tk_match_capture(): I32 => 185
  fun tk_match_dontcare(): I32 => 186

  fun tk_reference(): I32 => 187
  fun tk_packageref(): I32 => 188
  fun tk_typeref(): I32 => 189
  fun tk_typeparamref(): I32 => 190
  fun tk_typealiasref(): I32 => 191
  fun tk_newref(): I32 => 192
  fun tk_newberef(): I32 => 193
  fun tk_beref(): I32 => 194
  fun tk_funref(): I32 => 195
  fun tk_fvarref(): I32 => 196
  fun tk_fletref(): I32 => 197
  fun tk_embedref(): I32 => 198
  fun tk_tupleelemref(): I32 => 199
  fun tk_varref(): I32 => 200
  fun tk_letref(): I32 => 201
  fun tk_paramref(): I32 => 202
  fun tk_dontcareref(): I32 => 203
  fun tk_newapp(): I32 => 204
  fun tk_beapp(): I32 => 205
  fun tk_funapp(): I32 => 206
  fun tk_bechain(): I32 => 207
  fun tk_funchain(): I32 => 208

  fun tk_annotation(): I32 => 209

  fun tk_disposing_block(): I32 => 210

  // Pseudo tokens that never actually exist
  fun tk_newline(): I32 => 211  // Used by parser macros
  fun tk_flatten(): I32 => 212  // Used by parser macros for tree building

  // Token types for testing
  fun tk_test_no_seq(): I32 => 213
  fun tk_test_seq_scope(): I32 => 214
  fun tk_test_try_no_check(): I32 => 215
  fun tk_test_aliased(): I32 => 216
  fun tk_test_updatearg(): I32 => 217
  fun tk_test_extra(): I32 => 218

  fun string(token_id: TokenId): String val =>
    match token_id
    | 0 => "TK_EOF"
    | 1 => "TK_LEX_ERROR"
    | 2 => "TK_NONE"
    | 3 => "TK_TRUE"
    | 4 => "TK_FALSE"
    | 5 => "TK_STRING"
    | 6 => "TK_INT"
    | 7 => "TK_FLOAT"
    | 8 => "TK_ID"
    | 9 => "TK_LBRACE"
    | 10 => "TK_RBRACE"
    | 11 => "TK_LPAREN"
    | 12 => "TK_RPAREN"
    | 13 => "TK_LSQUARE"
    | 14 => "TK_RSQUARE"
    | 15 => "TK_BACKSLASH"
    | 16 => "TK_COMMA"
    | 17 => "TK_ARROW"
    | 18 => "TK_DBLARROW"
    | 19 => "TK_DOT"
    | 20 => "TK_TILDE"
    | 21 => "TK_CHAIN"
    | 22 => "TK_COLON"
    | 23 => "TK_SEMI"
    | 24 => "TK_ASSIGN"
    | 25 => "TK_PLUS"
    | 26 => "TK_PLUS_TILDE"
    | 27 => "TK_MINUS"
    | 28 => "TK_MINUS_TILDE"
    | 29 => "TK_MULTIPLY"
    | 30 => "TK_MULTIPLY_TILDE"
    | 31 => "TK_DIVIDE"
    | 32 => "TK_DIVIDE_TILDE"
    | 33 => "TK_REM"
    | 34 => "TK_REM_TILDE"
    | 35 => "TK_MOD"
    | 36 => "TK_MOD_TILDE"
    | 37 => "TK_AT"
    | 38 => "TK_AT_LBRACE"
    | 39 => "TK_LSHIFT"
    | 40 => "TK_LSHIFT_TILDE"
    | 41 => "TK_RSHIFT"
    | 42 => "TK_RSHIFT_TILDE"
    | 43 => "TK_LT"
    | 44 => "TK_LT_TILDE"
    | 45 => "TK_LE"
    | 46 => "TK_LE_TILDE"
    | 47 => "TK_GE"
    | 48 => "TK_GE_TILDE"
    | 49 => "TK_GT"
    | 50 => "TK_GT_TILDE"
    | 51 => "TK_EQ"
    | 52 => "TK_EQ_TILDE"
    | 53 => "TK_NE"
    | 54 => "TK_NE_TILDE"
    | 55 => "TK_PIPE"
    | 56 => "TK_ISECTTYPE"
    | 57 => "TK_EPHEMERAL"
    | 58 => "TK_ALIASED"
    | 59 => "TK_SUBTYPE"
    | 60 => "TK_QUESTION"
    | 61 => "TK_UNARY_MINUS"
    | 62 => "TK_UNARY_MINUS_TILDE"
    | 63 => "TK_ELLIPSIS"
    | 64 => "TK_CONSTANT"
    | 65 => "TK_LPAREN_NEW"
    | 66 => "TK_LSQUARE_NEW"
    | 67 => "TK_MINUS_NEW"
    | 68 => "TK_MINUS_TILDE_NEW"
    | 69 => "TK_COMPILE_INTRINSIC"
    | 70 => "TK_USE"
    | 71 => "TK_EXPORT"
    | 72 => "TK_TYPE"
    | 73 => "TK_INTERFACE"
    | 74 => "TK_TRAIT"
    | 75 => "TK_PRIMITIVE"
    | 76 => "TK_STRUCT"
    | 77 => "TK_CLASS"
    | 78 => "TK_ACTOR"
    | 79 => "TK_OBJECT"
    | 80 => "TK_LAMBDA"
    | 81 => "TK_BARELAMBDA"
    | 82 => "TK_AS"
    | 83 => "TK_IS"
    | 84 => "TK_ISNT"
    | 85 => "TK_VAR"
    | 86 => "TK_LET"
    | 87 => "TK_EMBED"
    | 88 => "TK_DONTCARE"
    | 89 => "TK_NEW"
    | 90 => "TK_FUN"
    | 91 => "TK_BE"
    | 92 => "TK_ISO"
    | 93 => "TK_TRN"
    | 94 => "TK_REF"
    | 95 => "TK_VAL"
    | 96 => "TK_BOX"
    | 97 => "TK_TAG"
    | 98 => "TK_CAP_READ"
    | 99 => "TK_CAP_SEND"
    | 100 => "TK_CAP_SHARE"
    | 101 => "TK_CAP_ALIAS"
    | 102 => "TK_CAP_ANY"
    | 103 => "TK_THIS"
    | 104 => "TK_RETURN"
    | 105 => "TK_BREAK"
    | 106 => "TK_CONTINUE"
    | 107 => "TK_CONSUME"
    | 108 => "TK_RECOVER"
    | 109 => "TK_IF"
    | 110 => "TK_IFDEF"
    | 111 => "TK_IFTYPE"
    | 112 => "TK_IFTYPE_SET"
    | 113 => "TK_THEN"
    | 114 => "TK_ELSE"
    | 115 => "TK_ELSEIF"
    | 116 => "TK_END"
    | 117 => "TK_WHILE"
    | 118 => "TK_DO"
    | 119 => "TK_REPEAT"
    | 120 => "TK_UNTIL"
    | 121 => "TK_FOR"
    | 122 => "TK_IN"
    | 123 => "TK_MATCH"
    | 124 => "TK_WHERE"
    | 125 => "TK_TRY"
    | 126 => "TK_TRY_NO_CHECK"
    | 127 => "TK_WITH"
    | 128 => "TK_ERROR"
    | 129 => "TK_COMPILE_ERROR"
    | 130 => "TK_NOT"
    | 131 => "TK_AND"
    | 132 => "TK_OR"
    | 133 => "TK_XOR"
    | 134 => "TK_DIGESTOF"
    | 135 => "TK_ADDRESS"
    | 136 => "TK_LOCATION"
    | 137 => "TK_PROGRAM"
    | 138 => "TK_PACKAGE"
    | 139 => "TK_MODULE"
    | 140 => "TK_MEMBERS"
    | 141 => "TK_FVAR"
    | 142 => "TK_FLET"
    | 143 => "TK_FFIDECL"
    | 144 => "TK_FFICALL"
    | 145 => "TK_IFDEFAND"
    | 146 => "TK_IFDEFOR"
    | 147 => "TK_IFDEFNOT"
    | 148 => "TK_IFDEFFLAG"
    | 149 => "TK_PROVIDES"
    | 150 => "TK_UNIONTYPE"
    | 151 => "TK_TUPLETYPE"
    | 152 => "TK_NOMINAL"
    | 153 => "TK_THISTYPE"
    | 154 => "TK_FUNTYPE"
    | 155 => "TK_LAMBDATYPE"
    | 156 => "TK_BARELAMBDATYPE"
    | 157 => "TK_DONTCARETYPE"
    | 158 => "TK_BOOL_TRUE"
    | 159 => "TK_BOOL_FALSE"
    | 160 => "TK_INFERTYPE"
    | 161 => "TK_ERRORTYPE"
    | 162 => "TK_LITERAL"
    | 163 => "TK_LITERALBRANCH"
    | 164 => "TK_OPERATORLITERAL"
    | 165 => "TK_TYPEPARAMS"
    | 166 => "TK_TYPEPARAM"
    | 167 => "TK_PARAMS"
    | 168 => "TK_PARAM"
    | 169 => "TK_TYPEARGS"
    | 170 => "TK_VALUEFORMALPARAM"
    | 171 => "TK_VALUEFORMALARG"
    | 172 => "TK_POSITIONALARGS"
    | 173 => "TK_NAMEDARGS"
    | 174 => "TK_NAMEDARG"
    | 175 => "TK_UPDATEARG"
    | 176 => "TK_LAMBDACAPTURES"
    | 177 => "TK_LAMBDACAPTURE"
    | 178 => "TK_SEQ"
    | 179 => "TK_QUALIFY"
    | 180 => "TK_CALL"
    | 181 => "TK_TUPLE"
    | 182 => "TK_ARRAY"
    | 183 => "TK_CASES"
    | 184 => "TK_CASE"
    | 185 => "TK_MATCH_CAPTURE"
    | 186 => "TK_MATCH_DONTCARE"
    | 187 => "TK_REFERENCE"
    | 188 => "TK_PACKAGEREF"
    | 189 => "TK_TYPEREF"
    | 190 => "TK_TYPEPARAMREF"
    | 191 => "TK_TYPEALIASREF"
    | 192 => "TK_NEWREF"
    | 193 => "TK_NEWBEREF"
    | 194 => "TK_BEREF"
    | 195 => "TK_FUNREF"
    | 196 => "TK_FVARREF"
    | 197 => "TK_FLETREF"
    | 198 => "TK_EMBEDREF"
    | 199 => "TK_TUPLEELEMREF"
    | 200 => "TK_VARREF"
    | 201 => "TK_LETREF"
    | 202 => "TK_PARAMREF"
    | 203 => "TK_DONTCAREREF"
    | 204 => "TK_NEWAPP"
    | 205 => "TK_BEAPP"
    | 206 => "TK_FUNAPP"
    | 207 => "TK_BECHAIN"
    | 208 => "TK_FUNCHAIN"
    | 209 => "TK_ANNOTATION"
    | 210 => "TK_DISPOSING_BLOCK"
    | 211 => "TK_NEWLINE"  // Used by parser macros
    | 212 => "TK_FLATTEN"  // Used by parser macros for tree building
    | 213 => "TK_TEST_NO_SEQ"
    | 214 => "TK_TEST_SEQ_SCOPE"
    | 215 => "TK_TEST_TRY_NO_CHECK"
    | 216 => "TK_TEST_ALIASED"
    | 217 => "TK_TEST_UPDATEARG"
    | 218 => "TK_TEST_EXTRA"
    else
      "TK_UNKNOWN"
    end

  fun is_entity(token_id: I32): Bool =>
    """
    Something like a class, actor, primitive, trait etc.

    Lambdas are not included. Maybe they should.
    """
    (token_id == this.tk_class()) or
      (token_id == this.tk_actor()) or
      (token_id == this.tk_primitive()) or
      (token_id == this.tk_struct()) or
      (token_id == this.tk_interface()) or
      (token_id == this.tk_trait()) or
      (token_id == this.tk_object())
