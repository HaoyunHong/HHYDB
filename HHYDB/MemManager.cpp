#include "MemManager.h"

inline void MemManager::link(int a, int b) {
    pool[a].next = b;
    pool[b].pre = a;
}

MemManager::MemManager() {
    num = 0;
    head = tail = -1;
}

void MemManager::access(int index) {

    if (index == head) {
        return;
    } else if (index == tail) {
        tail = pool[index].pre;
    } else {
        link(pool[index].pre, pool[index].next);
    }
    link(index, head);
    head = index;
}

int MemManager::alloc() {
    if (num == 0) {
        head = tail = num;
        return num++;
    } 
    if (num < 65536) {
        link(num, head);
        head = num;
        return num++;
    }
    access(tail);
    return head;
}