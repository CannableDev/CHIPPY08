#include "cstack.h"
#include <malloc.h>
#include <stdio.h>

void Cstack_Clean(Cstack* stack)
{
    while (stack->top)
    {
        Node* prev = stack->top;
        stack->top = stack->top->next;
        free(prev);
    }
    stack->top = NULL;
    stack->count = 0;
}

void Cstack_Push(Cstack* stack, uint16_t value)
{
    Node* n = malloc(sizeof(Node));
    n->next = stack->top;
    n->val = value;
    stack->top = n;
    stack->count++;
}

uint16_t Cstack_Pop(Cstack* stack)
{
    if (stack->count <= 0 || !stack->top)
    {
        perror("Stack is empty");
        return -1;
    }

    uint16_t val = stack->top->val;
    Node* n = stack->top;
    stack->top = stack->top->next;
    free(n);
    stack->count--;
}

uint16_t Cstack_Peek(Cstack* stack)
{
    if (stack->count <= 0 || !stack->top)
    {
        perror("Stack is empty");
        return -1;
    }

    return stack->top->val;
}