#include "subtype_ffi.h"
#include "assemble.h"
#include "subtype.h"

bool is_subtype_for_defs(ast_t* sub_def, token_id sub_cap,
  ast_t* super_def, token_id super_cap, pass_opt_t* opt)
{
  ast_t* sub_type =
    type_for_class(opt, sub_def, sub_def, sub_cap, TK_NONE, false);
  ast_setdata(sub_type, sub_def);

  ast_t* super_type =
    type_for_class(opt, super_def, super_def, super_cap, TK_NONE, false);
  ast_setdata(super_type, super_def);

  bool result = is_subtype(sub_type, super_type, NULL, opt);

  ast_free_unattached(sub_type);
  ast_free_unattached(super_type);

  return result;
}
