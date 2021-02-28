# pragma once

#include "BufPageManager.h"
#include "FileManager.h"
#include "Const.h"
#include "Database.h"
#include "BNode.h"
#include "Something.h"
#include "Somethings.h"
#include "Type.h"

using namespace std;
using json = nlohmann::json;

class Index {
    void up(int now);
    void down(int now);
    void insertRecord(Somethings* info, int RID, int now);
    void removeRecord(Somethings* info, int RID, int now);
    vector<uint> queryRecords(enumOp op, Somethings& data, int now);
    vector<int> queryRecord(Somethings* info, int now);
    uint queryRecordsNum(enumOp op, Somethings& data, int now);

public:
    Index() {}
    Index(Table* table, const char* name, vector<uint> key, int btree_max_per_node);
    Index(Table* table, json j);
    
    BNode* convert_buf_to_BNode(int index);
    void convert_BNode_to_buf(BNode* node);
    void Btree_remove(BNode* node);

    vector<uint> queryRecords(enumOp op, Somethings& data);
    vector<int> queryRecord(Somethings* info);
    uint queryRecordsNum(enumOp op, Somethings& data);
    void insertRecord(Somethings* info, int RID);
    void removeRecord(Somethings* info, int RID);

    void open();
    void remove();
    void close();
    json toJson();

    Table* table;
    char name[MAX_NAME_LEN];
    uint key_num;
    vector<uint> key;
    vector<uint> offset;
    uint page_num;
    uint root_page;
    uint next_del_page;
    uint16_t record_size;
    uint max_recond_num;
    int fileID;

    vector<dataType> ty;

    int btree_max_per_node;
    int btree_root_index;
};