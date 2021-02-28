# pragma once

#include "Const.h"
#include "BufPageManager.h"

class BufPageManager;

class Hash {
private:
    uint16_t table[65536 << 2];
    bool flag[65536 << 2];
    BufPageManager* bpm;
    int hash(int fileID, int pageID);

public:

    Hash(BufPageManager* bpm) : bpm(bpm) {
        memset(flag, 0, sizeof(flag));
    }
    ~Hash() {}
    int find(int fileID, int pageID);
    void add(int fileID, int pageID, int index);
    void del(int fileID, int pageID);
};