#include "Database.h"

#include "FileManager.h"
#include "BufPageManager.h"
#include "Print.h"
#include "Table.h"

using namespace std;

#ifdef WIN32
#include <direct.h>
#elif __linux__
#include <sys/types.h>
#include <sys/stat.h>
#endif

bool printStop;

#define filterTable(it) ((it) < 0 ? tempTable[-1-it] : table[it])

extern int cleanFiles(const char *dir);
extern char* readFile(const char* path);
extern void writeFile(const char* path, const char* data, const int length);
extern char* dirPath(const char* dir);
extern char* dirPath(const char* dir, const char* path);
extern char* dirPath(const char* dir, const char* filename, const char* filetype);
extern int checkFile(const char *filename);

Database::Database(const char* name, bool isCreate) {
    fileManager = new FileManager();
    bufManager = new BufPageManager(fileManager);
    memset(table, 0, sizeof(table));
    tableNum = 0;
    mem = 0;
    if (isCreate) {
        strcpy(this->name, name); 
        char* dbdir = dirPath(this->name);
        if (access("db", 0) == -1) {
            #ifdef WIN32
            mkdir("db");
            #elif __linux__
            mkdir("db", 0777);
            #endif
        }
        if (access(dbdir, 0) == -1) {
            #ifdef WIN32
            mkdir(dbdir);
            #elif __linux__
            mkdir(dbdir, 0777);
            #endif
        }
        update();
        fileManager->createFile(dirPath(name, ".storage"));
    } else {
        fromJson(json::parse(readFile(dirPath(name, "database.udb"))));
    }
    fileManager->openFile(dirPath(name, ".storage"), mem_file);
}

Database::~Database() {
    update();
    fileManager->closeFile(mem_file);
    for (int i = 0; i < 256; i++) if (table[i] != nullptr) {
        delete table[i];
    }
    delete fileManager;
    delete bufManager;
}

Table* Database::createTable(const char* name, int col_num, Type* col_ty) {
    Table* new_table = new Table();
    if (new_table->createTable(tableNum, this, name, col_num, col_ty, true)) {
        delete new_table;
        return nullptr;
    }
    table[tableNum++] = new_table;
    update();
    return new_table;
}

Table* Database::openTable(const char* name) {
    for (uint i = 0; i < tableNum; i++) {
        if (strcmp(table[i]->name, name) == 0) {
            return table[i];
        }
    }
    return nullptr;
}

int Database::deleteTable(const char* name) { // -1: doesn't exist, 0: OK
    int num = this->findTable(std::string(name));
    if (num == -1){
        printf("No table named %s\n", name);
        return -1;
    }
    delete table[num];
    for (uint i = num; i < tableNum - 1; i++) {
        table[i] = table[i + 1];
        table[i]->table_id = i;
    }
    table[--tableNum] = nullptr;
    fileManager->deleteFile(dirPath(this->name, name, "usid"));
    update();
    return 0;
}

void Database::showTables() {
    std::vector<std::pair<std::string, int> > v;
    v.push_back(std::pair<std::string, int>("Table", 20));
    Print::title(v);
    Somethings d;
    for (uint i = 0; i < tableNum; i++) {
        d.push_back(Something((char*)table[i]->name));
        Print::row(d);
        d.clear();
    }
    Print::end();
}
    
int Database::findTable(std::string s) {
    for (uint i = 0; i < tableNum; i++) {
        if (std::string(table[i]->name) == s) {
            return i;
        }
    }
    return -1;
}

char* Database::getVarchar(uint64_t idx) {
    uint page = idx >> 32;
    uint offset = (idx & 0xffffffff) >> 16;
    uint size = idx & 0xffff;
    BufType buf = bufManager->getPage(mem_file, page) + offset;
    char* str = new char[size + 1]; 
    char* _str = str;
    while (true) {
        if (PAGE_SIZE - offset > size) {
            memcpy(str, buf, size);
            offset += size;
            break;
        } else {
            memcpy(str, buf, (PAGE_SIZE - offset));
            str += PAGE_SIZE - offset;
            size -= PAGE_SIZE - offset;
            page += 1;
            offset = 0;
            buf = bufManager->getPage(mem_file, page) + offset;
        }
    }
    str[size] = '\0';
    return _str;
}

uint64_t Database::storeVarchar(char* str) {
    uint size = strlen(str);
    uint64_t out = (mem << 16) + size;
    int index;
    uint page = mem >> 16;
    uint offset = mem & 0xffff;
    BufType buf = bufManager->getPage(mem_file, page, index) + offset;
    while (true) {
        if (PAGE_SIZE - offset > size) {
            memcpy(buf, str, size);
            offset += size;
            break;
        } else {
            memcpy(buf, str, (PAGE_SIZE - offset));
            str += PAGE_SIZE - offset;
            size -= PAGE_SIZE - offset;
            page += 1;
            offset = 0;
            bufManager->markDirty(index);
            buf = bufManager->getPage(mem_file, page, index) + offset;
        }
    }
    bufManager->markDirty(index);
    mem = ((uint64_t)page << 16) + offset;
    return out;
}


// 增加 like % 模糊匹配
bool likeStr(Somethings &a, Somethings &b) {
    if (a.size() != b.size()) {
        return false;
    }
    uint sz = a.size();
    for (uint i = 0; i < sz; i++) {
        string a_str = a.v[i].anyRefCast<char*>();
        string b_str = b.v[i].anyRefCast<char*>();
        std::string regexStr;
        // 用一个状态机，循环构造 b 对应的正则表达式
        uint stat = 0;
        for (char i : b_str) {
            if (stat == 0) {
                if (i == '\\') {
                    stat = 1;
                } 
                else if (i == '[') {
                    regexStr += "[";
                    stat = 2;
                } 
                else if (i == '%') {
                    regexStr += ".*";
                } 
                else if (i == '_') {
                    regexStr += ".";
                } 
                else {
                    regexStr += i;
                }
            } 
            else if (stat == 1) {
                // 在 '\' 之后
                if (i == '%' || i == '_' || i == '!') {
                    regexStr += i;
                } 
                else {
                    regexStr += "\\";
                    regexStr += i;
                }
                stat = 0;
            } 
            else {
                if (i == '!') {
                    regexStr += "^";
                } else {
                    regexStr += i;
                }
                stat = 0;
            }
        }
        regex reg(regexStr);
        return regex_match(a_str, reg);
    }
}

bool cmpCol(enumOp op, Somethings &a, Somethings &b) {
    switch (op) {
    case OP_EQ: return a == b;
    case OP_NEQ: return a != b;
    case OP_LE: return a <= b;
    case OP_GE: return a >= b;
    case OP_LS: return a < b;
    case OP_GT: return a > b;
    case OP_LIKE: return likeStr(a, b);
    default: return false;
    }
}

bool Database::validWhere(WhereStatement &w) {
    Somethings data, _data;
    for (auto it: w.cols) {
        if (filterTable(it.first)->checkPointer() == false) return true;
        data.push_back(filterTable(it.first)->getPointerColData(it.second));
    }
    switch (w.rightV_ty) {
    case 0: {
        bool nl = true, nnl = true;
        for (uint i = 0; i < w.cols.size(); i++) {
            nl &= data[i].isNull();
            nnl &= !data[i].isNull();
        }
        return (w.op == OP_NL && nl) && (w.op == OP_NNL && nnl);
    }
    case 1: {
        for (auto it: w.rightV) {
            _data.push_back(it);
        }
        return cmpCol(w.op, data, _data);
    }
    case 2: {
        for (auto it: w.rightV_cols) {
            if (filterTable(it.first)->checkPointer() == false) return true;
            _data.push_back(filterTable(it.first)->getPointerColData(it.second));
        }
        return cmpCol(w.op, data, _data);
    }
    case 3:
        if (w.op == OP_IN) w.any = true, w.op = OP_EQ;
        if (w.any || w.all) {
            return filterTable(w.rightV_table)->cmpRecords(data, w.op, w.any, w.all);
        } else {
            return filterTable(w.rightV_table)->cmpRecord(data, filterTable(w.rightV_table)->val, w.op);
        }
    default:
        return false;
    }
}

void Database::storeData(uint idx) {
    Somethings data;
    if (sel[idx].select.size() == 0) {
        if (sel[idx].aggr.size() == 0) {
            for (auto it: sel[idx].from) {
                data.concatenate(filterTable(it.first)->getPointerData());
            }
        } else {
            for (auto it: sel[idx].aggr) if (it.ty != AG_COUNT) {
                data.push_back(filterTable(it.col.first)->getPointerColData(it.col.second));
            }
        }
    } else {
        for (auto it: sel[idx].select) {
            data.push_back(filterTable(it.first)->getPointerColData(it.second));
        }
    }
    if (data.size()) tempTable[idx]->data.push_back(data);
    tempTable[idx]->record_num += 1;
}

void Database::DFS(uint idx, uint f_idx, bool print = false) {
    for (auto it: sel[idx].where) {
        if (validWhere(it) == false) return;
    }
    if (f_idx == sel[idx].from.size()) {
        storeData(idx);
        if (print && tempTable[idx]->record_num % 50 == 0 && tempTable[idx]->printBack(50)) {
            puts("====== Enter c to continue, q to quit ======");
            while (true) {
                char ch = getch();
                if (ch == 'c') {
                    puts("================= Continue =================");
                    break;
                }
                if (ch == 'q') { 
                    printStop = true;
                    puts("=================== Quit ===================");
                    return; 
                }
            }
        }
    } else {
        filterTable(sel[idx].from[f_idx].first)->initPointer();
        while (filterTable(sel[idx].from[f_idx].first)->movePointer()) {
            DFS(idx, f_idx + 1, print);
            if (printStop) {
                filterTable(sel[idx].from[f_idx].first)->removePointer();
                return;
            }
        }
        filterTable(sel[idx].from[f_idx].first)->removePointer();
    }
}

void Database::buildSel(uint idx, bool print) {
    if (sel[idx].build) return; 
    sel[idx].build = true;
    for (auto it: sel[idx].recursion) {
        buildSel(-1-it);
    }
    printStop = false;
    for (uint i = 0; i < sel[idx].from.size(); i++) 
        filterTable(sel[idx].from[i].first)->removePointer();
    DFS(idx, 0, print);
    if (sel[idx].select.size() == 0 && sel[idx].aggr.size() > 0) {
        uint _idx = 0;
        // Table* table = tempTable[idx];
        for (auto it: sel[idx].aggr) switch (it.ty) {
            case AG_COUNT: {
                printf("COUNT: %d\n", (int)tempTable[idx]->record_num);
                // tempTable[idx]->val.push_back(Something((int)tempTable[idx]->record_num));
                break;
            }
            case AG_MIN: {
                Something min = Something();
                for (auto _it: tempTable[idx]->data) {
                    if (min.isNull()) {
                        min = _it[_idx];
                    } 
                    else if (_it[_idx].isNull()) {
                        continue;
                    }
                    else if (_it[_idx] < min) {
                        min = _it[_idx];
                    }
                }
                if (tempTable[idx]->col_ty[_idx].ty == INT) {
                    printf("MIN: %d\n", min.anyRefCast<int>());
                }
                else{
                    printf("MIN: %4LF\n", min.anyRefCast<long double>());
                }
                // tempTable[idx]->val.push_back(min);
                _idx++;
                break;
            }
            case AG_MAX: {
                Something max = Something();
                for (auto _it: tempTable[idx]->data) {
                    if (max.isNull()) {
                        max = _it[_idx];
                    } 
                    else if (_it[_idx].isNull()) {
                        continue;
                    } 
                    else if (_it[_idx] > max) {
                        max = _it[_idx];
                    }
                }
                if (tempTable[idx]->col_ty[_idx].ty == INT) {
                    printf("MAX: %d\n", max.anyRefCast<int>());
                }
                else{
                    printf("MAX: %4LF\n", max.anyRefCast<long double>());
                }
                // tempTable[idx]->val.push_back(max);
                _idx++;
                break;
            }
            case AG_SUM: 
            case AG_AVG: {
                if (tempTable[idx]->col_ty[_idx].ty == INT) {
                    int sum = 0;
                    int num = 0;
                    for (auto _it: tempTable[idx]->data) {
                        if (_it[_idx].isNull() == false) {
                            num++;
                            sum += _it[_idx].anyRefCast<int>();
                        }
                    }
                    if (it.ty == AG_SUM) {
                        printf("SUM: %d\n", sum);
                        tempTable[idx]->val.push_back(Something(sum));
                    }
                    else {
                        // 格式化输出
                        printf("AVG: %4Lf\n", (long double)sum / num);
                        // tempTable[idx]->val.push_back(Something((long double)sum / num));
                    }
                } 
                else {
                    long double sum = 0;
                    int num = 0;
                    for (auto _it: tempTable[idx]->data) {
                        if (_it[_idx].isNull() == false) {
                            num++;
                            sum += _it[_idx].anyRefCast<long double>();
                        }
                    }
                    if (it.ty == AG_SUM) {
                        printf("SUM: %4Lf\n", sum);
                        // tempTable[idx]->val.push_back(Something(sum));
                    } 
                    else {
                        printf("AVG: %4Lf\n", (long double)sum / num);
                        // tempTable[idx]->val.push_back(Something(sum / num));
                    }
                }
                _idx++;
                break;
            }
        }
        return;
    }
    uint _idx = 0;

    printf("tempTable[idx]->record_num: %d\n", tempTable[idx]->record_num);
    if (print && printStop == false && tempTable[idx]->record_num % 50 && tempTable[idx]->printBack(tempTable[idx]->record_num % 50) == false) {
        tempTable[idx]->print();
    }
}

void showDatabases() {
    std::vector<std::pair<std::string, int> > v;
    v.push_back(std::pair<std::string, int>("Database", 20));
    Print::title(v);
    Somethings d;

    DIR *dirp;
    struct dirent *dp;
    struct stat dir_stat;
    
    stat("db", &dir_stat);
    if (S_ISDIR(dir_stat.st_mode)) {
        dirp = opendir("db");
        while ((dp=readdir(dirp)) != NULL) {
            if ((0 == strcmp(".", dp->d_name)) || (0 == strcmp("..", dp->d_name))) { // Ignore . & ..
                continue;
            }
            stat(dp->d_name, &dir_stat);
            if (S_ISDIR(dir_stat.st_mode)) {
                if (checkFile(dirPath(dp->d_name, ".storage")) == 1) {
                    d.push_back(Something((char*)dp->d_name));
                    Print::row(d);
                    d.clear();
                }
            }
        }
        closedir(dirp);
    }
    
    Print::end();
}

int cleanDatabase(const char *dbname) {
    return cleanFiles(dirPath(dbname));
}

json Database::toJson() {
    json j;
    j["name"] = name;
    j["tableNum"] = tableNum;
    j["mem"] = mem;
    for (uint i = 0; i < tableNum; i++) {
        j["table"].push_back(table[i]->toJson());
    }
    return j;
}

void Database::fromJson(json j) {
    strcpy(name, j["name"].get<std::string>().c_str());
    tableNum = j["tableNum"].get<int>();
    mem = j["mem"].get<uint64_t>();
    for (uint i = 0; i < tableNum; i++) {
        table[i] = new Table(this, j["table"][i]);
    }
}

void Database::update() {
    std::string j = toJson().dump(4);
    writeFile(dirPath(name, "database.udb"), j.c_str(), j.length());
}

