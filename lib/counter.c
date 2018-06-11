#include <stdlib.h>
#include "counter.h"

struct counter {
    int val;
};

counter_t *counter_create(int start)
{
    counter_t *c;

    c = malloc(sizeof(*c));
    c->val = start;

    return c;
}

void counter_destroy(counter_t *c)
{
    if (c == NULL)
        return;
    free(c);
}

void counter_add(counter_t *c, int amount)
{
    if (c == NULL)
        return;
    c->val += amount;
}

void counter_subtract(counter_t *c, int amount)
{
    if (c == NULL)
        return;
    c->val -= amount;
}

void counter_increment(counter_t *c)
{
    if (c == NULL)
        return;
    c->val++;
}

void counter_decrement(counter_t *c)
{
    if (c == NULL)
        return;
    c->val--;
}

int counter_getval(counter_t *c)
{
    if (c == NULL)
        return 0;
    return c->val;
}
