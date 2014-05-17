#ifndef STRINGTAB_H
#define STRINGTAB_H

#include "list.h"

DECLARE_LIST(strlist, const char);

const char* stringtab(const char* string);
void stringtab_done();

#endif
