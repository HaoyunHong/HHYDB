# pragma once

#include "Const.h"
#include "Somethings.h"
using namespace std;

class BRecord{
public:
    BRecord() {}
    BRecord(int RID, Somethings key) : RID(RID) , key(key) {}
    int RID;
    Somethings key;
};

class BNode{
public:
    BNode() {
        this->isLeaf = true;
        this->fatherID = -1;
        this->RCount = 0;
        this->record.clear();
        this->childs.clear();
        this->childs.push_back(-1);
    };

    std::vector<int> childs;
    int index;
    int RCount;
    std::vector<BNode*> childPointers;
    std::vector<BRecord> record;
    int fatherID;
    int leftPID;
    int rightPID;
    BNode *leftPage;
    BNode *rightPage;
    bool isLeaf;
};