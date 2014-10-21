#ifndef UNIT_TYPE_AST_H
#define UNIT_TYPE_AST_H

#define NOMINAL(name) NOMINAL_CAP(name, val)
#define NOMINAL_CAP(name, cap) \
  "(nominal{dataref " #name "} x (id " #name ") x " #cap " x)"

extern const char* t_bool;
extern const char* t_u_lit;
extern const char* t_s_lit;
extern const char* t_f_lit;
extern const char* t_none;
extern const char* t_true;
extern const char* t_false;
extern const char* t_u8;
extern const char* t_u16;
extern const char* t_u32;
extern const char* t_u64;
extern const char* t_u128;
extern const char* t_i8;
extern const char* t_i16;
extern const char* t_i32;
extern const char* t_i64;
extern const char* t_i128;
extern const char* t_f32;
extern const char* t_f64;
extern const char* t_string;
extern const char* t_t1;
extern const char* t_t2;
extern const char* t_t3;
extern const char* t_foo;
extern const char* t_bar;
extern const char* t_structural;
extern const char* builtin;

#endif
