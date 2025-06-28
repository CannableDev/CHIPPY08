#ifndef CSTACK_H
#define CSTACK_H

#include <stdint.h>

typedef struct Node Node;

struct Node
{
	Node* next;
	uint16_t val;
};

typedef struct Cstack
{
	Node* top;
	size_t count;
} Cstack;

void Cstack_Clean(Cstack* stack);
void Cstack_Push(Cstack* stack, uint16_t value);
uint16_t Cstack_Pop(Cstack* stack);
uint16_t Cstack_Peek(Cstack* stack);

#endif 