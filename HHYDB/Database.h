# pragma once

#include "FileManager.h"
#include "BufPageManager.h"
#include "Const.h"
#include "Type.h"
#include "Somethings.h"

using namespace std;
using json = nlohmann::json;


class Table;

#define filterTable(it) ((it) < 0 ? tempTable[-1-it] : table[it])

bool likeStr(Somethings &a, Somethings &b);
int cleanDatabase(const char *dbname);
void showDatabases();

class Database {
    bool validWhere(WhereStatement &w);
    json toJson();
    void fromJson(json j);
    void storeData(uint idx);
    void DFS(uint idx, uint f_idx, bool print);
    
public:
    Database(const char* name, bool create);
    ~Database();
    Table* createTable(const char* name, int col_num, Type* col_ty);
    int findTable(std::string s);
    int deleteTable(const char* name);
    Table* openTable(const char* name);
    void showTables();
    // 重新建表
    void buildSel(uint idx, bool print = false);
    // 可变长字符串
    char* getVarchar(uint64_t idx);
    uint64_t storeVarchar(char* str);
    void update();

    FileManager* fileManager;
    BufPageManager* bufManager;
    char name[MAX_NAME_LEN];
    Table* table[256];
    uint tableNum;
    int mem_file;
    uint64_t mem;

    SelectStatement sel[256]; 
    Table* tempTable[256];
    uint sel_num = 0;
};