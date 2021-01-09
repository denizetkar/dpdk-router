#include "pointer_list.h"
#include <stdio.h>
#include <stdlib.h>

//---------'pointer_list' FUNCTIONS----------------------
void pointer_list_init(pointer_list_ptr self)
{
    self->_list = (generic_ptr *)malloc(0);
    self->_len = 0;
    self->_cap = 0;
}

static void _pointer_list_realloc(pointer_list_ptr self, unsigned int new_cap)
{
    self->_list = (generic_ptr *)realloc(self->_list, new_cap * sizeof(generic_ptr));
    if (self->_list == NULL)
    {
        printf("ERROR: Unable to allocate memory!\n");
        exit(EXIT_FAILURE);
    }
    self->_cap = new_cap;
}

static void _pointer_list_expand(pointer_list_ptr self)
{
    unsigned int new_cap = self->_cap + self->_cap / 2 + 1;
    if (new_cap <= self->_cap)
    {
        printf("ERROR: Pointer list cannot grow larger!\n");
        exit(EXIT_FAILURE);
    }
    _pointer_list_realloc(self, new_cap);
}

static unsigned int _pointer_list_contracted_cap(pointer_list_ptr self)
{
    if (self->_cap <= 0)
        return 0;

    unsigned int new_cap;
    if (self->_cap < 3)
        new_cap = self->_cap - 1;
    else
        new_cap = self->_cap - self->_cap / 3;

    return new_cap;
}

static void _pointer_list_contract(pointer_list_ptr self, unsigned int min_cap)
{
    unsigned int new_cap = _pointer_list_contracted_cap(self);
    new_cap = (new_cap < min_cap) ? min_cap : new_cap;
    if (new_cap >= self->_cap)
        return;

    _pointer_list_realloc(self, new_cap);
}

void pointer_list_append(pointer_list_ptr self, generic_ptr ptr)
{
    if (self->_len >= self->_cap)
    {
        _pointer_list_expand(self);
    }
    self->_list[self->_len] = ptr;
    self->_len++;
}

void pointer_list_remove(pointer_list_ptr self, unsigned int index)
{
    if (index >= self->_len)
        return;
    for (; index < self->_len - 1; index++)
    {
        self->_list[index] = self->_list[index + 1];
    }
    self->_len--;

    if (self->_len <= _pointer_list_contracted_cap(self))
        _pointer_list_contract(self, 0);
}

generic_ptr pointer_list_pop(pointer_list_ptr self)
{
    if (self->_len <= 0)
        return NULL;
    generic_ptr last = self->_list[self->_len - 1];
    pointer_list_remove(self, self->_len - 1);
    return last;
}

generic_ptr pointer_list_get(const pointer_list_ptr self, unsigned int index)
{
    if (index >= self->_len)
        return NULL;
    return self->_list[index];
}

void pointer_list_set(pointer_list_ptr self, unsigned int index, generic_ptr ptr)
{
    if (index >= self->_len)
        return;
    self->_list[index] = ptr;
}

unsigned int pointer_list_len(const pointer_list_ptr self)
{
    return self->_len;
}

unsigned int pointer_list_cap(const pointer_list_ptr self)
{
    return self->_cap;
}

void pointer_list_insert(pointer_list_ptr self, generic_ptr ptr, unsigned int pos)
{
    if (pos > pointer_list_len(self))
        return;
    pointer_list_append(self, NULL);
    unsigned int i, sz = pointer_list_len(self);
    generic_ptr curr = ptr, next;
    for (i = pos; i < sz; i++)
    {
        next = self->_list[i];
        self->_list[i] = curr;
        curr = next;
    }
}

void pointer_list_clear(pointer_list_ptr self)
{
    if (self->_list == NULL)
        return;
    free(self->_list);
    self->_list = NULL;
    self->_len = 0;
    self->_cap = 0;
}

void pointer_list_deep_clear(pointer_list_ptr self)
{
    unsigned int i, len;
    // Clean up all of the interface configurations.
    len = pointer_list_len(self);
    for (i = 0; i < len; i++)
        free(pointer_list_get(self, i));
    pointer_list_clear(self);
}

void pointer_list_print(const pointer_list_ptr self, printer_fn fn)
{
    unsigned int i, len;

    len = pointer_list_len(self);
    for (i = 0; i < len; i++)
    {
        fn(pointer_list_get(self, i));
    }
}

// Note that comparison_fn is going to get elements of type 'const generic_ptr *'
// in order to compare. So, you must first dereference them to get the generic
// pointers that you appended to the list!
void pointer_list_sort(pointer_list_ptr self, comparison_fn cmp)
{
    qsort(self->_list, self->_len, sizeof(generic_ptr), cmp);
}
