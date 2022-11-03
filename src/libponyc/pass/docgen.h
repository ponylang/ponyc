#ifndef DOCGEN_H
#define DOCGEN_H

#include <platform.h>
#include "../ast/ast.h"
#include "pass.h"
#include "../logo.h"

PONY_EXTERN_C_BEGIN

#define PONYLANG_MKDOCS_ASSETS_DIR "assets/"
#define PONYLANG_MKDOCS_CSS_FILE "ponylang.css"
#define PONYLANG_MKDOCS_LOGO_FILE "logo.png"
#define PONYLANG_MKDOCS_CSS "[data-md-color-scheme=\"ponylang\"] {\n"\
                            "  --md-typeset-a-color: var(--md-primary-fg-color);\n"\
                            "}"


// Generate docs for the given program
void generate_docs(ast_t* program, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
