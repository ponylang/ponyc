#ifndef STRINGTAB_H
#define STRINGTAB_H

#include <platform.h>
#include "list.h"

PONY_EXTERN_C_BEGIN

DECLARE_LIST(strlist, const char);

const char* stringtab(const char* string);
void stringtab_done();

PONY_EXTERN_C_END

#endif
