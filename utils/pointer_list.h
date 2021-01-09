#ifndef __POINTER_LIST_H
#define __POINTER_LIST_H

#include <stdint.h>
#include <stddef.h>

#include "utils.h"

typedef struct pointer_list
{
    generic_ptr *_list;
    unsigned int _len;
    unsigned int _cap;
} pointer_list, *pointer_list_ptr;
typedef int (*comparison_fn)(const void *, const void *);

void pointer_list_init(pointer_list_ptr);
void pointer_list_append(pointer_list_ptr, generic_ptr ptr);
void pointer_list_remove(pointer_list_ptr, unsigned int index);
generic_ptr pointer_list_pop(pointer_list_ptr);
generic_ptr pointer_list_get(const pointer_list_ptr, unsigned int index);
void pointer_list_set(pointer_list_ptr, unsigned int index, generic_ptr ptr);
unsigned int pointer_list_len(const pointer_list_ptr);
unsigned int pointer_list_cap(const pointer_list_ptr);
void pointer_list_insert(pointer_list_ptr, generic_ptr ptr, unsigned int pos);
void pointer_list_clear(pointer_list_ptr);
void pointer_list_deep_clear(pointer_list_ptr);

void pointer_list_print(const pointer_list_ptr, printer_fn fn);
void pointer_list_sort(pointer_list_ptr, comparison_fn cmp);

#endif