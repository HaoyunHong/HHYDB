%{
#include "Const.h"
#include "parser.h"
#include "Table.h"
#include "Database.h"
#include "Type.h"
#include "Print.h"
using namespace std;

extern "C"
{
    void yyerror(const char *s);
    extern int yylex(void);
};

// 月份约束
uint mothDays[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// in FileManager
extern int checkFile(const char *filename);
extern char* dirPath(const char* dir, const char* path);

extern Database* currentDB;
bool error;

bool hasUseDB() {
    if (currentDB == nullptr) {
        printf("Please select a database first\n");
        return false;
    }
    else{
        return true;
    }
}

bool hasTable(string name, int& tableID, bool expect) {
    tableID = currentDB->findTable(name);
    if (tableID == -1 && expect) {
        printf("TABLE %s doesn't exist in current database!\n", name.c_str());
    }
    if (tableID != -1 && !expect) {
        printf("TABLE %s exists\n", name.c_str());
    }
    return tableID != -1;
}

// 对多表链接和查询要做修改
int2uint dealCol(str2str col, vector<int2str> &from, bool isTbs=false) {
    for (auto _it: from) {
        if ((col.first == _it.second) || (col.first == string() && from.size() == 1)) {
            int colIndex = filterTable(_it.first)->findCol(col.second);
            if (colIndex < 0) {
                if (!isTbs){
                    printf("COLUMN %s doesn't exist in TABLE %s\n", col.second.c_str(), _it.second.c_str());
                    error = true;
                }
                return int2uint(-1, 0);
            }
            return int2uint(_it.first, (uint)colIndex);
        }
    }
    
    printf("TABLE %s doesn't exist\n", col.first.c_str());
    error = true;
    return int2uint(0, 0);
}

inline vector<int2uint> dealCols(vector<str2str> &cols, vector<int2str> &from, bool isTbs=false) {
	vector<int2uint> v;
    vector<int2str> _from;
    // // 此处处理多表连接，只能1列按顺序对应1表
    // 现在可以按笛卡尔积去匹配了
    if(isTbs){
        /* if(cols.size() != from.size()){
            printf("Multitable link size mismatch!\n");
        }*/
        
        for(auto tb: from){
            _from.push_back(tb);   
            for(auto col: cols){   
                int2uint _tempCol = dealCol(col, _from, isTbs);
                // 如果返回的first是-1，那就说明没找到匹配的，就不会存入
                if(_tempCol.first<0){
                    continue;
                }
                else{
                    v.push_back(_tempCol); 
                }   
            }
            _from.clear();
        }
    } 
    else{
        for (auto col: cols) {
            v.push_back(dealCol(col, from, isTbs));
        }
    }
	return v;
}

void dealTy(uint idx) {
	if (currentDB->sel[idx].select.size() == 0) {
        if (currentDB->sel[idx].aggr.size() == 0) {
            for (auto it: currentDB->sel[idx].from) {
                Table* st = filterTable(it.first);
                for (uint i = 0; i < st->col_num; i++) {
                    currentDB->tempTable[idx]->col_ty[currentDB->tempTable[idx]->col_num++] = st->col_ty[i];
                }
            }
        } else {
           for (auto it: currentDB->sel[idx].aggr) switch (it.ty) {
                case AG_COUNT: { 
                   break;
                }
                case AG_MAX:
                case AG_MIN: {
                    break;
                }
                case AG_SUM: 
                case AG_AVG: {
                    if (filterTable(it.col.first)->col_ty[it.col.second].ty != INT && filterTable(it.col.first)->col_ty[it.col.second].ty != DECIMAL) {
                        printf("Use %s for %s is illegal\n", Aggr2Str(it.ty).c_str(), Type2Str(filterTable(it.col.first)->col_ty[it.col.second].ty).c_str());
                        error = true;
                        return;
                    }
                    break;
                }
            }
        }
    } else {
        for (auto it: currentDB->sel[idx].select) {
            currentDB->tempTable[idx]->col_ty[currentDB->tempTable[idx]->col_num++] = filterTable(it.first)->col_ty[it.second];
        }
    }
}

inline bool typeCheck(Type ty, Something v) {
    switch (ty.ty) {
        case INT: 
            return v.anyCast<int>() != nullptr;
        case CHAR: 
        case VARCHAR: 
            return (v.anyCast<char*>() != nullptr) && (strlen((const char*)v.anyCast<char*>()) <= ty.char_len);
        case DATE: 
            return v.anyCast<uint32_t>() != nullptr;
        case DECIMAL: 
            return v.anyCast<long double>() != nullptr;
        default:
            return false;
    }
}

inline bool cmpType(Type ty, Type _ty) {
    if ((ty.ty == _ty.ty) || (ty.ty == VARCHAR && _ty.ty == CHAR) || (ty.ty == CHAR && _ty.ty == VARCHAR)) {
        return true;
    }
    return false;
}

inline const char* op2str(enumOp op) {
    switch (op) {
    case OP_EQ: return "=";   
    case OP_NEQ: return "<>";
    case OP_LE: return "<=";
    case OP_GE: return ">=";    
    case OP_LS: return "<";    
    case OP_GT: return ">";    
    case OP_NL: return "IS NULL";   
    case OP_NNL: return "IS NOT NULL";   
    case OP_IN: return "IN";
    case OP_LIKE: return "LIKE";
    default: return "";
    }
}

WhereStatement dealWhere(YYType_Where &it, vector<int2str> &from, bool isTbs=false) {
    WhereStatement where;
    // 得到where表达式中的cols是哪个表的
    where.cols = dealCols(it.cols, from, isTbs);
    // 说明并没有在表from中找到这个col
    if(where.cols[0].first < 0){
        return where;
    }
    where.op = it.op;
    where.rightV = it.rightV;
    where.rightV_cols = dealCols(it.rightV_cols, from, isTbs);
    where.rightV_table = it.rightV_table;
    where.any = (it.ty == 5);
    where.all = (it.ty == 6);
    switch (it.ty) {
    case 0:
        where.rightV_ty = 1;
        if (where.cols.size() != where.rightV.size()) {
            printf("length is not equal\n");
            break;
        }
        for (uint i = 0, size = where.cols.size(); i < size; i++) {
            if (typeCheck(int2uint2Col(where.cols[i]), where.rightV[i]) == false) {
                printf("Type error: %s.%s(%s) %s %s\n", filterTable(where.cols[i].first)->name, int2uint2Col(where.cols[i]).name, Type2Str(int2uint2Col(where.cols[i]).ty).c_str(),op2str(where.op), Any2Str(where.rightV[i]).c_str());
                error = true;
            }
        }
        break;
    case 1:
        where.rightV_ty = 2;
        if (where.cols.size() != where.rightV_cols.size()) {
            printf("length is not equal\n");
            break;
        }
        for (uint i = 0, size = where.cols.size(); i < size; i++) {
            if (cmpType(int2uint2Col(where.cols[i]), int2uint2Col(where.rightV_cols[i])) == false) {
                printf("Type error: %s.%s(%s) %s %s.%s(%s)\n", filterTable(where.cols[i].first)->name, int2uint2Col(where.cols[i]).name, Type2Str(int2uint2Col(where.cols[i]).ty).c_str(),op2str(where.op), filterTable(where.rightV_cols[i].first)->name, int2uint2Col(where.rightV_cols[i]).name,Type2Str(int2uint2Col(where.rightV_cols[i]).ty).c_str());
                error = true;
            }
        }
        break;
    case 2:
    case 3:
        where.rightV_ty = 0;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        where.rightV_ty = 3;
        if (it.ty == 4 && filterTable(where.rightV_table)->sel != 2) {
            printf("use %s for TABLE is illegal\n", op2str(where.op));
            break;
        }
        if (it.ty > 4 && filterTable(where.rightV_table)->sel == 2) {
            printf("use %s for VALUELIST is illegal\n", op2str(where.op));
            break;
        }
        if (where.cols.size() != filterTable(where.rightV_table)->col_num) {
            printf("length is not equal\n");
            break;
        }
        for (uint i = 0, size = where.cols.size(); i < size; i++) {
            if (cmpType(int2uint2Col(where.cols[i]), filterTable(where.rightV_table)->col_ty[i]) == false) {
                printf("Type error: %s.%s(%s) %s %s.%s(%s)\n", filterTable(where.cols[i].first)->name, int2uint2Col(where.cols[i]).name, Type2Str(int2uint2Col(where.cols[i]).ty).c_str(),op2str(where.op), filterTable(where.rightV_table)->name, filterTable(where.rightV_table)->col_ty[i].name,Type2Str(filterTable(where.rightV_table)->col_ty[i].ty).c_str());
                error = true;
            }
        }
        break;
    }
    return where;
}

bool checkData(Type tar, Something val) {
    if (val.anyCast<char*>() != nullptr) {
        return tar.ty == CHAR || tar.ty == VARCHAR;
    } else if (val.anyCast<int>() != nullptr) {
        return tar.ty == INT;
    } else if (val.anyCast<long double>() != nullptr) {
        return tar.ty == DECIMAL;
    } else if (val.anyCast<uint32_t>() != nullptr) {
        return tar.ty == DATE;
    }
    else return true;
}

%}

%token DATABASE DATABASES TABLE TABLES CREATE SHOW DROP USE PRIMARY KEY NOT NL INSERT INTO VALUES DELETE FROM WHERE UPDATE SET SELECT IS TYPE_INT TYPE_VARCHAR TYPE_CHAR DEFAULT CONSTRAINT CHANGE ALTER ADD RENAME DESC REFERENCES INDEX AND TYPE_DATE TYPE_FLOAT FOREIGN LIKE EXIT ON TO LB RB SEMI COMMA STAR EQ NEQ LE GE LS GT DOT IN ANY ALL AS COUNT MIN MAX AVG SUM
%token <S> IDENTIFIER
%token <I> VALUE_INT
%token <S> VALUE_STRING
%token <U> VALUE_DATE
%token <D> VALUE_LONG_DOUBLE
%type <S> dbName tbName colName pkName fkName idxName aliasName
%type <TY> type
%type <TY> field
%type <V_TY> fields
%type <V> value
%type <V_V> values _values
%type <P_SS> col
%type <V_P_SS> selector cols _cols
%type <I> seleStatement
%type <P_IS> fromClause
%type <V_P_IS> fromClauses
%type <W> whereClause
%type <V_W> whereClauses
%type <eOP> op
%type <V_S> colNames
%type <V_P_SA> setClause
%type <G> aggrClause
%type <V_G> aggrClauses _aggrClauses

%%

program:
    | EXIT {
        if (currentDB != nullptr)delete currentDB;
        return 0;
    }
    | stmt program;

stmt:
    error
    | sysStatement
    | dbStatement
    | tbStatement
    | idxStatement
    | alterStatement;

sysStatement:
    SHOW DATABASES SEMI {
        showDatabases();
    };

dbStatement:
    CREATE DATABASE dbName SEMI {
        if (checkFile(dirPath($3.c_str(), ".storage")) == 0) {
            new Database($3.c_str(), true);   
        } else {
            printf("DATABASE %s exists\n", $3.c_str());
        }
    }
    | DROP DATABASE dbName SEMI {
        if (checkFile(dirPath($3.c_str(), ".storage")) == 1) {
            if (currentDB && strcmp(currentDB->name, $3.c_str()) == 0) {
                delete currentDB;
                currentDB = nullptr;
            }
            cleanDatabase($3.c_str());
        } else {
            printf("DATABASE %s doesn't exist\n", $3.c_str());
        }
    }
    | USE dbName SEMI {
        if (checkFile(dirPath($2.c_str(), ".storage")) == 1) {
            if (currentDB) delete currentDB;
            currentDB = new Database($2.c_str(), false);
        } else {
            printf("DATABASE %s doesn't exist\n", $2.c_str());
        }
    }
    | SHOW TABLES SEMI {
        if (hasUseDB()) {
            currentDB->showTables();
        }
    };

tbStatement:
    CREATE TABLE tbName LB fields RB SEMI {
        if (hasUseDB()) {
            int tableID;
            if (!hasTable($3, tableID, false)) {
                Type* ty = new Type[$5.size()];
                bool flag = true;
                for (uint i = 0; i < $5.size(); i++) {
                    ty[i] = $5[i];
                    for (uint j = 0; j < i; j++) {
                        if (string(ty[i].name) == string(ty[j].name)) {
                            flag = false;
                            break;
                        }
                    }
                    if (!flag) break;
                }
                if (flag) {
                    currentDB->createTable($3.c_str(), $5.size(), ty);
                    currentDB->update();
                }
                else {
                    printf("Column name conflict\n");
                }
            }
        }
    }
    | DROP TABLE tbName SEMI {
        if (hasUseDB()) {
            if (currentDB->deleteTable($3.c_str())) printf("TABLE %s doesn't exist\n", $3.c_str());
        }
    }
    | DESC tbName SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($2, tableID, true)) {
                currentDB->table[tableID]->printCol();
            }
        }
    }
    | INSERT INTO tbName VALUES LB values RB SEMI{
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->col_num == $6.size()) {
                    bool flag = true;
                    for (uint i = 0; i < $6.size(); i++) {
                        if (!checkData(currentDB->table[tableID]->col_ty[i], $6[i])) {
                            flag = false;
                            break;
                        }
                    }
                    if (flag) {
                        Something* temp = new Something[currentDB->table[tableID]->col_num];
                        for (uint i = 0; i < $6.size(); i++)temp[i] = $6[i];
                        int x = currentDB->table[tableID]->insertRecord(temp);
                        if (x != 0)printf("Insert fail %d\n",x);
                    }
                    else printf("Data type mismatch\n");
                }
                else printf("Column number doesn't match\n");
            }
            currentDB->update();
        }
    }
    | INSERT INTO tbName LB colNames RB VALUES LB values RB SEMI{
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if ($5.size() == $9.size()) {
                    vector <uint> key;
                    bool hasColName = true;
                    for (uint i = 0; i < $5.size(); i++) {
                        bool flag = false;
                        for (uint j = 0; j < currentDB->table[tableID]->col_num; j++) {
                            if ($5[i] == string(currentDB->table[tableID]->col_ty[j].name)) {
                                flag = true;
                                key.push_back(j);
                                break;
                            }
                        }
                        if (!flag) {
                            hasColName = false;
                            printf("TABLE %s doesn't have a column named %s\n", $3.c_str(), $5[i].c_str());
                            break;
                        }
                    }
                    if (hasColName) {
                        bool flag = true;
                        for (uint i = 0; i < key.size(); i++) {
                            if (!checkData(currentDB->table[tableID]->col_ty[key[i]], $9[i])) {
                                flag = false;
                                break;
                            }
                        }
                        if (flag) {
                            Something* temp = new Something[currentDB->table[tableID]->col_num];
                            for (uint i = 0; i < $9.size(); i++)temp[key[i]] = $9[i];
                            int x = currentDB->table[tableID]->insertRecord(temp);
                            if (x != 0)printf("Insert fail %d\n",x);
                        }
                        else printf("Data type mismatch\n");
                    }
                }
                else printf("Column number doesn't match\n");
            }
            currentDB->update();
        }
    }
    | DELETE FROM tbName WHERE whereClauses SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                error = false;
                vector<int2str> from; 
                from.push_back(int2str(tableID, $3));
                vector<WhereStatement> where;
                for (auto it: $5) {
                    where.push_back(dealWhere(it, from));
                }
                if (error) {
                    printf("Where statement not found\n");
                    YYERROR;
                }
                for (auto it: where) {
                    if (it.rightV_table < 0) {
                        currentDB->buildSel(-1 - it.rightV_table);
                    }
                }
                currentDB->table[tableID]->removeRecords(where);
            }
        }
    }
    | UPDATE tbName SET setClause WHERE whereClauses SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($2, tableID, true)) {
                error = false;
                vector<int2str> from; 
                from.push_back(int2str(tableID, $2));
                vector<int2something> set;
                for (auto it: $4) {
                    int colIndex = currentDB->table[tableID]->findCol(it.first);
                    if (colIndex < 0) {
                        printf ("COLUMN %s doesn't exist in TABLE %s\n", $2.c_str(), it.first.c_str());
                        error = true;
                    } else {
                        set.push_back(int2something(colIndex, it.second));
                    }
                }
                vector<WhereStatement> where;
                for (auto it: $6) where.push_back(dealWhere(it, from));
                if (error) {
                    YYERROR;
                }
                for (auto it: where) {
                    if (it.rightV_table < 0) {
                        currentDB->buildSel(-1 - it.rightV_table);
                    }
                }
                currentDB->table[tableID]->updateRecords(set, where);
            }
        }
    }
    | seleStatement SEMI {
        if (hasUseDB()) {
            currentDB->buildSel(-1 - $1, true);
            while (currentDB->sel_num) {
                delete currentDB->tempTable[--(currentDB->sel_num)];
                currentDB->sel[currentDB->sel_num].build = false;
            }
        }
    };

seleStatement:
    SELECT selector FROM fromClauses {
        if (hasUseDB()) {
            for (auto it: $4) {
                if (it.first == 256) {
                    YYERROR;
                }
            }
            error = false;
            uint idx = currentDB->sel_num++;
            currentDB->tempTable[idx] = new Table(1);
            if ($4.size()>1){
                currentDB->sel[idx].select = dealCols($2, $4, true);
            }
            else{
                currentDB->sel[idx].select = dealCols($2, $4);
            }
            currentDB->sel[idx].aggr.clear();
            currentDB->sel[idx].from = $4;
            currentDB->sel[idx].recursion.clear();
            for (auto it: $4) if (it.first < 0) {
                currentDB->sel[idx].recursion.push_back(it.first);
            }
            currentDB->sel[idx].where.clear();
            dealTy(idx);
            $$ = -1 - idx;
            
            if (error) {
                delete currentDB->tempTable[--currentDB->sel_num];
                YYERROR;
            }
        } 
        else {
            YYERROR;
        }
    }
    | SELECT _aggrClauses FROM fromClauses {
        if (hasUseDB()) {
            for (auto it: $4) {
                if (it.first == 256) {
                    YYERROR;
                }
            }
            error = false;
            uint idx = currentDB->sel_num++;
            currentDB->tempTable[idx] = new Table(2);
            currentDB->sel[idx].select.clear();
            currentDB->sel[idx].aggr.clear();
            for (auto it: $2) if (it.ty == AG_COUNT) {
                currentDB->sel[idx].aggr.push_back(AggrStatement{it.ty, int2uint(0, 0)});
            } else {
                currentDB->sel[idx].aggr.push_back(AggrStatement{it.ty, dealCol(it.col, $4)});
            }
            currentDB->sel[idx].from = $4;
            currentDB->sel[idx].recursion.clear();
            for (auto it: $4) if (it.first < 0) currentDB->sel[idx].recursion.push_back(it.first);
            currentDB->sel[idx].where.clear();
            dealTy(idx);
            $$ = -1 - idx;
            if (error) {
                delete currentDB->tempTable[--currentDB->sel_num];
                YYERROR;
            }
        } 
        else {
            YYERROR;
        }
    }
    | SELECT selector FROM fromClauses WHERE whereClauses {
        if (hasUseDB()) {
            for (auto it: $4) {
                if (it.first == 256) {
                    YYERROR;
                }
            }
            error = false;
            uint idx = currentDB->sel_num++;
            currentDB->tempTable[idx] = new Table(1);
            if($4.size()>1){
                currentDB->sel[idx].select = dealCols($2, $4, true);
            }
            else{
                currentDB->sel[idx].select = dealCols($2, $4);
            }
            currentDB->sel[idx].aggr.clear();
            currentDB->sel[idx].from = $4;
            currentDB->sel[idx].recursion.clear();
            for (auto it: $4) {
                if (it.first < 0) {
                    currentDB->sel[idx].recursion.push_back(it.first);
                }
            }
            currentDB->sel[idx].where.clear();
            vector<int2str> _from;
            // 调用的函数会遍历tables的
            // 多表 WHERE
            if ($4.size() > 1){
                for (auto it: $6) {
                    currentDB->sel[idx].where.push_back(dealWhere(it, $4, true));
                }
            }
            else{
                for (auto it: $6) {
                    currentDB->sel[idx].where.push_back(dealWhere(it, $4));
                }
            }
            for (auto it: currentDB->sel[idx].where) {
                if (it.rightV_table < 0) {
                    currentDB->sel[idx].recursion.push_back(it.rightV_table);
                }
            }
            dealTy(idx);
            $$ = -1 - idx;
            if (error) {
                delete currentDB->tempTable[--currentDB->sel_num];
                printf("Not found\n");
                YYERROR;
            }
        } 
        else {
            printf("Not found\n");
            YYERROR;
        }
    }
    | SELECT _aggrClauses FROM fromClauses WHERE whereClauses {
        if (hasUseDB()) {
            for (auto it: $4) if (it.first == 256) YYERROR;
            error = false;
            uint idx = currentDB->sel_num++;
            currentDB->tempTable[idx] = new Table(2);
            currentDB->sel[idx].select.clear();
            currentDB->sel[idx].aggr.clear();
            for (auto it: $2) if (it.ty == AG_COUNT) {
                currentDB->sel[idx].aggr.push_back(AggrStatement{it.ty, int2uint(0, 0)});
            } else {
                currentDB->sel[idx].aggr.push_back(AggrStatement{it.ty, dealCol(it.col, $4)});
            }
            currentDB->sel[idx].from = $4;
            currentDB->sel[idx].recursion.clear();
            for (auto it: $4) if (it.first < 0) {
                currentDB->sel[idx].recursion.push_back(it.first);
            }
            currentDB->sel[idx].where.clear();
            for (auto it: $6) currentDB->sel[idx].where.push_back(dealWhere(it, $4));
            for (auto it: currentDB->sel[idx].where) if (it.rightV_table < 0) {
                currentDB->sel[idx].recursion.push_back(it.rightV_table);
            }
            dealTy(idx);
            $$ = -1 - idx;
            if (error) {
                delete currentDB->tempTable[--currentDB->sel_num];
                YYERROR;
            }
        } else YYERROR;
    };

idxStatement:
    CREATE INDEX idxName ON tbName LB colNames RB SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($5, tableID, true)) {
                if (currentDB->table[tableID]->findIndex($3) != -1) {
                    printf("INDEX %s exists\n", $3.c_str());
                }
                else{
                    vector<uint> key_index;
                    bool create_ok = true;
                    for (uint i = 0; i < $7.size(); i++) {
                        bool flag = false;
                        for (uint j = 0; j < currentDB->table[tableID]->col_num; j++) {
                            if ($7[i] == string(currentDB->table[tableID]->col_ty[j].name)) {
                                flag = true;
                                key_index.push_back(j);
                                break;
                            }
                        }
                        if (!flag) {
                            printf("TABLE %s doesn't have a column named %s\n", $5.c_str(), $7[i].c_str());
                            create_ok = false;
                            break;
                        }
                    }
                    if (create_ok) {
                        currentDB->table[tableID]->createIndex(key_index, $3);
                    }
                }
            }
            currentDB->update();
        }
    }
    | DROP INDEX idxName SEMI {
        if (hasUseDB()) {
            bool flag = false;
            for (uint i = 0; i < currentDB->tableNum; i++) {
                int indexID = currentDB->table[i]->findIndex($3);
                if (indexID != -1) {
                    currentDB->table[i]->removeIndex(indexID);
                    flag = true;
                    break;
                }
            }
            if (!flag) {
                printf("Index %s doesn't exist\n", $3.c_str());
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName ADD INDEX idxName LB colNames RB SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->findIndex($6) != -1) {
                    printf("INDEX %s exists\n", $6.c_str());
                }
                else{
                    vector<uint> key_index;
                    bool create_ok = true;
                    for (uint i = 0; i < $8.size(); i++) {
                        bool flag = false;
                        for (uint j = 0; j < currentDB->table[tableID]->col_num; j++) {
                            if ($8[i] == string(currentDB->table[tableID]->col_ty[j].name)) {
                                flag = true;
                                key_index.push_back(j);
                                break;
                            }
                        }
                        if (!flag) {
                            printf("TABLE %s doesn't have a column named %s\n", $3.c_str(), $8[i].c_str());
                            create_ok = false;
                            break;
                        }
                    }
                    if (create_ok) {
                        currentDB->table[tableID]->createIndex(key_index, $6);
                        printf("INDEX %s added\n", $6.c_str());
                    }
                }
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName DROP INDEX idxName SEMI{
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                int indexID = currentDB->table[tableID]->findIndex($6);
                if (indexID != -1) {
                    currentDB->table[tableID]->removeIndex(indexID);
                    printf("INDEX %s dropped\n", $6.c_str());
                }
                else {
                    printf("INDEX %s doesn't exist\n", $6.c_str());
                }
            }
            currentDB->update();
        }
    };

alterStatement:
    ALTER TABLE tbName ADD field SEMI{
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->findCol(string($5.name)) != -1) {
                    printf("Column %s is used\n", $5.name);
                }
                else {
                    currentDB->table[tableID]->createColumn($5);
                }
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName DROP colName SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->findCol($5) != -1)currentDB->table[tableID]->removeColumn(currentDB->table[tableID]->findCol($5));
                else printf("Column %s doesn't exist\n", $5.c_str());
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName CHANGE colName field SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->findCol($5) != -1) {
                    if (currentDB->table[tableID]->findCol(string($6.name)) == -1 || $5 == string($6.name)){
                        currentDB->table[tableID]->modifyColumn(currentDB->table[tableID]->findCol($5), $6);
                    }
                    else {
                        printf("Column %s is used\n", $6.name);
                    }
                }
                else {
                    printf("Column %s doesn't exist\n", $5.c_str());
                }
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName RENAME TO tbName SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                for (uint i = 0; i < $6.length(); i++) {
                    currentDB->table[tableID]->name[i] = $6[i];
                }
                currentDB->table[tableID]->name[$6.length()] = '\0';
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName ADD PRIMARY KEY LB colNames RB SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->p_key != nullptr){
                    printf("TABLE %s has a primary key\n", $3.c_str());
                }
                else {
                    int* key_index = new int[$8.size()];
                    bool create_ok = true;
                    for (uint i = 0; i < $8.size(); i++) {
                        bool flag = false;
                        for (uint j = 0; j < currentDB->table[tableID]->col_num; j++) {
                            if ($8[i] == string(currentDB->table[tableID]->col_ty[j].name)) {
                                flag = true;
                                key_index[i] = j;
                                break;
                            }
                        }
                        if (!flag) {
                            printf("TABLE %s doesn't have a column named %s\n", $3.c_str(), $8[i].c_str());
                            create_ok = false;
                            break;
                        }
                    }
                    if (create_ok) {
                        PrimaryKey temp = PrimaryKey(currentDB->table[tableID], $8.size(), key_index, $8[0].c_str());
                        int x = currentDB->table[tableID]->createPrimaryKey(&temp);
                        if (x == -2){
                            printf("Constraint problem\n");
                        }
                    }
                }
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName DROP PRIMARY KEY SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->p_key == nullptr) {
                    printf("TABLE %s doesn't have a primary key\n", $3.c_str());
                }
           	    else {
                    currentDB->table[tableID]->removePrimaryKey();
                }
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName ADD CONSTRAINT pkName PRIMARY KEY LB colNames RB SEMI {
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                if (currentDB->table[tableID]->p_key != nullptr)printf("TABLE %s has a primary key\n", $3.c_str());
                else {
                    int* key_index = new int[$10.size()];
                    bool create_ok = true;
                    for (uint i = 0; i < $10.size(); i++) {
                        bool flag = false;
                        for (uint j = 0; j < currentDB->table[tableID]->col_num; j++) {
                            if ($10[i] == string(currentDB->table[tableID]->col_ty[j].name)) {
                                flag = true;
                                key_index[i] = j;
                                break;
                            }
                        }
                        if (!flag) {
                            printf("TABLE %s doesn't have a column named %s\n", $3.c_str(), $10[i].c_str());
                            create_ok = false;
                            break;
                        }
                    }
                    if (create_ok) {
                        PrimaryKey temp = PrimaryKey(currentDB->table[tableID], $10.size(), key_index, $6.c_str());
                        int x = currentDB->table[tableID]->createPrimaryKey(&temp);
                        if (x == -2) {
                            printf("Constraint problem\n");
                        }
                    }
                }
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName DROP PRIMARY KEY pkName SEMI{
        // 这里也可以再改改
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) { 
                if(currentDB->table[tableID]->p_key->name == $7) {
                    currentDB->table[tableID]->removePrimaryKey(currentDB->table[tableID]->p_key);
                }
                else{
                    printf("TABLE %s doesn't have a primary key\n", $3.c_str());
                }
                /* if (currentDB->table[tableID]->p_key == nullptr) {
                    printf("TABLE %s doesn't have a primary key\n", $3.c_str());
                }
                else {
                    currentDB->table[tableID]->removePrimaryKey();
                } */
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName ADD CONSTRAINT fkName FOREIGN KEY LB colNames RB REFERENCES tbName LB colNames RB SEMI {
        if (hasUseDB()) {
            int tableID1;
            int tableID2;
            if (hasTable($3, tableID1, true) && hasTable($13, tableID2, true)) {
                if (currentDB->table[tableID2]->p_key == nullptr){
                    printf("TABLE %s doesn't have a primary key\n", $13.c_str());
                }
                else if (currentDB->table[tableID2]->p_key->v.size() == $15.size()) {
                    bool isok = true;
                    for (uint i = 0; i < currentDB->table[tableID2]->p_key->v.size(); i++) {
                        if ($15[i] != string(currentDB->table[tableID2]->col_ty[currentDB->table[tableID2]->p_key->v[i]].name)) {
                            isok = false;
                            printf("Primary key column %s mismatch column name %s\n", currentDB->table[tableID2]->col_ty[currentDB->table[tableID2]->p_key->v[i]].name, $15[i].c_str());
                            break;
                        }
                    }
                    if (isok) {
                        bool flag = true;
                        vector <uint> key;
                        for (uint i = 0; i < $10.size(); i++) {
                            bool flag2 = false;
                            for (uint j = 0; j < currentDB->table[tableID1]->col_num; j++) {
                                if ($10[i] == string(currentDB->table[tableID1]->col_ty[j].name)) {
                                    key.push_back(j);
                                    flag2 = true;
                                    break;
                                }
                            }
                            if (!flag2) {
                                flag = false;
                                printf("Column %s doesn't exist\n", $10[i].c_str());
                                break;
                            }
                        }
                        if (flag) {
                            int* temp = new int[$10.size()];
                            for (uint i = 0; i < $10.size(); i++) {
                                temp[i] = key[i];
                            }
                            ForeignKey fkey = ForeignKey(currentDB->table[tableID1], $10.size(), temp, $6.c_str());
                            currentDB->table[tableID1]->createForeignKey(&fkey, currentDB->table[tableID2]->p_key);
                        }
                    }
                }
                else {
                    printf("Column num of target primary key mismatch\n");
                }
            }
            currentDB->update();
        }
    }
    | ALTER TABLE tbName DROP FOREIGN KEY fkName SEMI{
        if (hasUseDB()) {
            int tableID;
            if (hasTable($3, tableID, true)) {
                bool flag = false;
                for (uint i = 0; i < currentDB->table[tableID]->f_key.size(); i++) {
                    if (currentDB->table[tableID]->f_key[i]->name == $7) {
                        currentDB->table[tableID]->removeForeignKey(currentDB->table[tableID]->f_key[i]);
                        flag = true;
                        break;
                    }
                }
                if (!flag) {
                    printf("TABLE %s doesn't have a foreign key named %s\n", $3.c_str(), $7.c_str());
                }
            }
            currentDB->update();
        }
    };

fields:
    field {
        $$.push_back($1);
    }
    | fields COMMA field {
        $1.push_back($3);
        $$ = $1;
    };

field:
    colName type {
        strcpy($2.name, $1.c_str());
        $$ = $2;
    }
    | colName type NOT NL {
        strcpy($2.name, $1.c_str());
        $2.setNull(false);
        $$ = $2;
    }
    | colName type DEFAULT value {
        strcpy($2.name, $1.c_str());
        $2.sth = $4;
        $$ = $2;
    }
    | colName type NOT NL DEFAULT value  {
        strcpy($2.name, $1.c_str());
        $2.setNull(false);
        $2.sth = $6;
        $$ = $2;
    };

type:
    TYPE_INT {
        $$ = Type("", INT);
    }
    | TYPE_VARCHAR LB VALUE_INT RB {
        $$ = Type("", VARCHAR, $3);
    }
    | TYPE_CHAR LB VALUE_INT RB {
        $$ = Type("", CHAR, $3);
    }
    | TYPE_DATE {
        $$ = Type("", DATE);
    }
    | TYPE_FLOAT {
        $$ = Type("", DECIMAL);
    };

_values:
    value { 
        $$.push_back($1);
    }
    | LB values RB { 
        $$ = $2;
    };

values:
    value { 
        $$.push_back($1);
    }
    | values COMMA value {
        $1.push_back($3);
        $$ = $1;
    };

value:
    VALUE_INT { $$ = Something($1); }
    | VALUE_STRING { 
        char* str = new char[$1.length() + 1];
        memcpy(str, $1.c_str(), $1.length());
        str[$1.length()] = '\0';
        $$ = Something(str);
    }
    | VALUE_DATE { 
        if (($1 / 10000 < 1500 || $1 / 10000 > 2500) || 
            ($1 % 10000 / 100 < 1 || $1 % 10000 / 100 > 12) || 
            ($1 % 100 < 1 || $1 % 100 > mothDays[$1 % 10000 / 100 - 1]) || 
            ((($1 / 10000 % 4 != 0) ^ ($1 / 10000 % 100 != 0) ^ ($1 / 10000 % 400 != 0)) && $1 % 100 == 29)) {
            printf("Wrong date: %04d-%02d-%02d\n", $1 / 10000, $1 % 10000 / 100, $1 % 100);
            YYERROR;
        }
        $$ = Something($1); 
    }
    | VALUE_LONG_DOUBLE { $$ = Something($1); }
    | NL { $$ = Something(); };

fromClauses:
    fromClause {
        $$.push_back($1);
    }
    | fromClauses COMMA fromClause {
        $1.push_back($3);
        $$ = $1;
    };

fromClause:
    tbName {
        int idx = (currentDB ? currentDB->findTable($1) : 256);
        if (idx < 0) {
            idx = 256;
            printf("Can not find TABLE %s\n", $1.c_str());
        }
        $$ = make_pair(idx, $1);
    }
    | tbName AS aliasName {
        int idx = (currentDB ? currentDB->findTable($1) : 256);
        if (idx < 0) {
            idx = 256;
            printf("Can not find TABLE %s\n", $1.c_str());
        }
        $$ = make_pair(idx, $3);
    }
    | LB seleStatement RB AS aliasName {
        $$ = make_pair($2, $5);
    };

aliasName:
    IDENTIFIER { $$ = $1; };

whereClauses:
    whereClause {
        $$.push_back($1);
    }
    | whereClauses AND whereClause {
        $1.push_back($3);
        $$ = $1;
    };

whereClause:
    _cols op _values {
        $$.ty = 0;
        $$.cols = $1;
        $$.op = $2;
        $$.rightV = $3;
    }
    | _cols op _cols {
        $$.ty = 1;
        $$.cols = $1;
        $$.op = $2;
        $$.rightV_cols = $3;
    }
    | _cols IS NL {
        $$.ty = 2;
        $$.cols = $1;
        $$.op = OP_NL;
    }
    | _cols IS NOT NL {
        $$.ty = 3;
        $$.cols = $1;
        $$.op = OP_NNL;
    }
    | _cols op LB seleStatement RB {
        $$.ty = 4;
        $$.cols = $1;
        $$.op = $2;
        $$.rightV_table = $4;
    }
    | _cols IN LB seleStatement RB {
        $$.ty = 7;
        $$.cols = $1;
        $$.op = OP_IN;
        $$.rightV_table = $4;
    }
    | _cols LIKE _values{
        $$.ty = 0;
        $$.cols = $1;
        $$.op = OP_LIKE;
        $$.rightV = $3;
    };

_cols:
    LB cols RB {
        $$ = $2;
    }
    | col {
        $$.push_back($1);
    };

cols:
    cols COMMA col {
        $1.push_back($3);
        $$ = $1;
    }
    | col {
        $$.push_back($1);
    };

col:
    tbName DOT colName {
        $$ = make_pair($1, $3);
    }
    | colName {
        $$ = make_pair(string(), $1);
    };

op:
    EQ { $$ = OP_EQ; }
    | LIKE { $$ = OP_LIKE; }
    | NEQ { $$ = OP_NEQ; }
    | LE { $$ = OP_LE; }
    | GE { $$ = OP_GE; }
    | LS { $$ = OP_LS; }
    | GT { $$ = OP_GT; };

setClause:
    colName EQ value {
        $$.push_back(make_pair($1, $3));
    }
    | setClause COMMA colName EQ value {
        $1.push_back(make_pair($3, $5));
        $$ = $1;
    };

selector:
    STAR {}
    | cols { $$ = $1; };

_aggrClauses:
    LB aggrClauses RB {
        $$ = $2;
    }
    | aggrClause {
        $$.push_back($1);
    };

aggrClauses:
    aggrClauses COMMA aggrClause {
        $1.push_back($3);
        $$ = $1;
    }
    | aggrClause {
        $$.push_back($1);
    };

aggrClause:
    COUNT LB STAR RB{
        $$.ty = AG_COUNT;
    }
    | MIN LB col RB{
        $$.ty = AG_MIN;
        $$.col = $3;
    }
    | MAX LB col RB{
        $$.ty = AG_MAX;
        $$.col = $3;
    }
    | SUM LB col RB{
        $$.ty = AG_SUM;
        $$.col = $3;
    }
    | AVG LB col RB{
        $$.ty = AG_AVG;
        $$.col = $3;
    };

colNames:
    colName {
        $$.push_back($1);
    }
    | colNames COMMA colName {
        $1.push_back($3);
        $$ = $1;
    };

dbName:
    IDENTIFIER { $$ = $1; };

tbName:
    IDENTIFIER { $$ = $1; };

colName:
    IDENTIFIER { $$ = $1; };

pkName:
    IDENTIFIER { $$ = $1; };

fkName:
    IDENTIFIER { $$ = $1; };

idxName:
    IDENTIFIER { $$ = $1; };
%%

void yyerror(const char* s)
{
    cerr << s << endl;
}
