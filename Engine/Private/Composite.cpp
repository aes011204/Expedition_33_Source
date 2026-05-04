#include "Composite.h"

Composite::Composite()
{
}

Composite::~Composite()
{
}

void Composite::Add_Child(Shared<class BNode> child)
{
    _children.push_back(child);
}

