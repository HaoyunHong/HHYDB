# pragma once

#include "Const.h"

class MemManager {
private:
    struct node {
        int pre, next;
    };
    node pool[65536];
    int num, head, tail;

    void link(int a, int b);

public:
    MemManager();
    void access(int index);
    int alloc();
};