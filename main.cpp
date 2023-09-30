#include <stdio.h>
#include <math.h>

#include "stack.h"
#include "debug.h"


int main()
{
    LGIN;

    Stack stk = {};
    StackInit(&stk);

    for (size_t i = 0; i < 10; i++)
    {
        Push(&stk,10);
    }

    StackDtor(&stk);

    LGOUT;

    return 0;
}
