# pragma once

#include "Const.h"
#include "Type.h"
#include "Index.h"
#include "Key.h"
#include "Somethings.h"
#include "Database.h" // enumOp

using namespace std;

using json = nlohmann::json;

class Database;
class FileManager;
class BufPageManager;

string Type2Str(dataType ty);

class Table {
private:
    void insert(Something& val, dataType ty, uint size, BufType& buf);
    void fetch(BufType& buf, dataType ty, uint size, Something& val);
    void fetchWithOffset(BufType& buf, dataType ty, uint size, Something& val, uint offset);
    uint genOffset(uint index);
    char* getStr(BufType buf, uint size);
    
    std::vector<uint> existentRecords();
    bool validWhere(Somethings data, WhereStatement &w);
    bool checkWheres(Somethings data, std::vector<WhereStatement> &where);
    std::vector<uint> findWheres(std::vector<WhereStatement> &where);
    Somethings chooseData(Somethings& data, std::vector<uint>& cols);

    struct Pointer {
        Table* s;
        uint pid;
        BufType buf;
        void init();
        bool next();
        Pointer() {}
        Pointer(Table* _s, uint _pid);
        Somethings get();
    } pointer;
    
public:
    char name[MAX_NAME_LEN];
    Database* db;
    FileManager* fileManager;
    BufPageManager* bufManager;
    uint table_id;
    uint col_num = 0;
    Type col_ty[64];
    PrimaryKey* p_key = nullptr; // primary key
    std::vector<ForeignKey*> f_key; // foreign keys
    int p_key_index = -1;
    std::vector<int> f_key_index;
    uint index_num = 0;
    Index index[256];
    uint record_num = 0; // all record, include removed record
    int main_file;
    uint record_size;
    uint record_onepg;
    
    uint sel = 0;
    std::vector<Somethings> data; // when sel = 1
    Somethings val;   // when sel = 2

    json toJson();
    Table(Database* db, json j);

    Table() { this->pointer.s = this; }
    Table(uint _sel) : sel(_sel) { this->pointer.s = this; }
    ~Table();
    uint calDataSize();
    int createTable(uint table_id, Database* db, const char* name, uint col_num, Type* col_ty, bool create = false);
    int insertRecord(Something* data);
    // void removeRecord(const int len, Something* info);
    int removeRecord(const int RID);
    Somethings queryRecord(const int RID);
    int queryRecord(const int RID, Something* &data);
    // int updateRecord(const int RID, const int len, Something* data);
    int updateRecord(const int RID, std::vector<int2something> &set);

    int removeRecords(std::vector<WhereStatement> &where);
    int updateRecords(std::vector<int2something> &set, std::vector<WhereStatement> &where);

    bool cmpRecord(Somethings a, Somethings b, enumOp op);
    bool cmpRecords(Somethings data, enumOp op, bool any, bool all);

    int findIndex(std::string s);
    uint createIndex(std::vector<uint> col_id, std::string name);
    // uint createKeyIndex(Key* key); 
    void removeIndex(uint index_id);

    int createColumn(Type ty);
    int removeColumn(uint col_id);
    int modifyColumn(uint col_id, Type ty);
    void updateColumns();

    void rebuild(int ty, uint col_id);

    int createForeignKey(ForeignKey* fk, PrimaryKey* pk);
    int removeForeignKey(ForeignKey* fk);
    int createPrimaryKey(PrimaryKey* pk);
    int removePrimaryKey();
    int removePrimaryKey(PrimaryKey* pk);
    // bool queryPrimaryKey(Something* info); 

    void print();
    void printCol();
    bool printBack(uint num);

    int findCol(std::string a);
    int constraintCol(uint col_id);
    int constraintKey(Key* key);
    int constraintRow(Something* data, uint RID, bool ck_unique);
    int constraintRowKey(Something* data, Key* key);

    void initPointer();
    void removePointer();
    bool movePointer();
    bool checkPointer();
    Something getPointerColData(uint idx);
    Somethings getPointerData();
    uint getPointer();
};