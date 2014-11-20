#ifndef STRINGTAB_H
#define STRINGTAB_H

#include <platform.h>
#include "../../libponyrt/ds/list.h"

PONY_EXTERN_C_BEGIN

DECLARE_LIST(strlist, const char);

void stringtab_init();
const char* stringtab(const char* string);
void stringtab_done();

PONY_EXTERN_C_END

#endif
