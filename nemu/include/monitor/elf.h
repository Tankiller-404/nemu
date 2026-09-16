#ifndef __MONITOR_ELF_H__
#define __MONITOR_ELF_H__

#include "common.h"

bool elf_get_variable(const char *name, uint32_t *value);
bool elf_find_function(uint32_t address, const char **name, uint32_t *start);

#endif
