# pragma once

#include "Const.h"
#include "parser.h"
#include "Something.h"
#include "FileManager.h"
#include "BufPageManager.h"
#include "Database.h"
#include "Table.h"
#include "Type.h"
#include "Index.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <assert.h>
using namespace std;

// 是全局变量
Database* currentDB;

inline uint32_t date2uint(string str) {
    string year = str.substr(0, 4);
    string month = str.substr(5, 1);
    string placeHolder = "0";
    string day;
    // 还是要补齐成 1996-01-02 的形式，否则后面的处理会很麻烦
    int len = str.length();
    if(str.substr(6, 1).compare("-")==0){
        month = placeHolder+month;
        if(len==8){
            day = placeHolder + str.substr(7, 1);
        }
        else{
            day = str.substr(7, 2);
        }
    }
    else{
        month = str.substr(5, 1);
        if(len==9){
            day = placeHolder + str.substr(8, 1);
        }
        else{
            day = str.substr(8, 2);
        }
    }
    str = year + month + day;
    // 以这种uint形式存储，然后以加横杠的行书输出
    return (uint32_t)atoi(str.c_str());
}

inline char* copyChars(const char* s) {
    int len = strlen(s);
    char* str = new char[len + 1];
    strcpy(str, s);
    str[len] = '\0';
    return str;
}

void importOneTable(Table* table, const char* filename, char delimeter) {

    printf("%s\n",filename);

    // 用这种方式读入
    ifstream inputFile(filename, ios::in);
    string line;
    string field;

    Something* data = new Something[table->col_num];
    // 跳掉第一行
    int lineId = 1;
    getline(inputFile, line);
    lineId ++;

    while (getline(inputFile, line)) {

        istringstream sin(line); 

        memset(data, 0, table->col_num * sizeof(Something));

        for (uint i = 0; i < table->col_num; i++) {

            getline(sin, field, delimeter);  

            switch (table->col_ty[i].ty) {

            case INT:
                data[i] = atoi(field.c_str());
                break;
            
            case CHAR:
            case VARCHAR:
                data[i] = copyChars(field.c_str());
                break;
            
            case DATE:
                data[i] = date2uint(field);
                break;
            
            case DECIMAL:
                data[i] = strtold(field.c_str(), NULL);
                break;
            }
        }

        int rid = table->insertRecord(data);
        if (rid != 0){
            printf("Insert record failed\n");
        }
        lineId++;
    }
    delete[] data;
    inputFile.close();
}

void importTablesData(char delimeter) {
    // 创建存储测试数据的数据库
    cleanDatabase("testdb");
    Database *db = new Database("testdb", true);

    // 表指针
    Table* tablePointer;
    // 然后让主键们都 NOT NULL
    // table: 0
    tablePointer = db->createTable("part", 9, new Type[9]{
        Type("p_partkey", INT, 0, NULL, false), 
        Type("p_name", VARCHAR, 55), 
        Type("p_mfgr", CHAR, 25),
        Type("p_brand", CHAR, 10), 
        Type("p_type", VARCHAR, 25), 
        Type("p_size", INT), 
        Type("p_container", CHAR, 10), 
        Type("p_retailprice", DECIMAL), 
        Type("p_comment", VARCHAR, 23),
    });
    // ==0 是创建成功的意思  new int[1]{0} 是右值的0的意思
    tablePointer->createPrimaryKey(new PrimaryKey(tablePointer, 1, new int[1]{0}, "p_partkey"));
    importOneTable(tablePointer, "data/part.tbl.csv", delimeter);

    // table: 1
    tablePointer = db->createTable("region", 3, new Type[3]{
        Type("r_regionkey", INT, 0, NULL, false), 
        Type("r_name", CHAR, 25), 
        Type("r_comment", VARCHAR, 152),
    });
    tablePointer->createPrimaryKey(new PrimaryKey(tablePointer, 1, new int[1]{0}, "r_regionkey"));
    importOneTable(tablePointer, "data/region.tbl.csv", delimeter);

    // table: 2
    tablePointer = db->createTable("nation", 4, new Type[4]{
        Type("n_nationkey", INT, 0, NULL, false), 
        Type("n_name", CHAR, 25), 
        Type("n_regionkey", INT, 0, NULL, false),
        Type("n_comment", VARCHAR, 152),
    });
    tablePointer->createPrimaryKey(new PrimaryKey(tablePointer, 1, new int[1]{0}, "n_nationkey"));
    tablePointer->createForeignKey(new ForeignKey(tablePointer, 1, new int[1]{2}, "n_regionkey"), db->table[1]->p_key);
    importOneTable(tablePointer, "data/nation.tbl.csv", delimeter);

    // // old table: 3
    // tablePointer = db->createTable("supplier", 7, new Type[7]{
    //     Type("s_suppkey", INT), 
    //     Type("s_name", CHAR, 25), 
    //     Type("s_address", VARCHAR, 40),
    //     Type("s_nationkey", INT, 0, NULL, false),
    //     Type("s_phone", CHAR, 15),
    //     Type("s_acctbal", DECIMAL),
    //     Type("s_comment", VARCHAR, 101),
    // });

    // tablePointer->createPrimaryKey(new PrimaryKey(tablePointer, 1, new int[1]{0}, "s_suppkey\0"));
    // tablePointer->createForeignKey(new ForeignKey(tablePointer, 1, new int[1]{3}, "s_nationkey\0"), db->table[2]->p_key);
    // importOneTable(tablePointer, "data/supplier.tbl.csv", '|');

    // table: 3
    tablePointer = db->createTable("customer", 8, new Type[8]{
        Type("c_custkey", INT, 0, NULL, false), 
        Type("c_name", CHAR, 25), 
        Type("c_address", VARCHAR, 40),
        Type("c_nationkey", INT, 0, NULL, false),
        Type("c_phone", CHAR, 15),
        Type("c_acctbal", DECIMAL),
        Type("c_mktsegment", CHAR, 10),
        Type("c_comment", VARCHAR, 117),
    });
    tablePointer->createPrimaryKey(new PrimaryKey(tablePointer, 1, new int[1]{0}, "c_custkey"));
    tablePointer->createForeignKey(new ForeignKey(tablePointer, 1, new int[1]{3}, "c_nationkey\0"), db->table[2]->p_key);
    importOneTable(tablePointer, "data/customer.tbl.csv", delimeter);

    // table: 4
    tablePointer = db->createTable("partsupp", 5, new Type[5]{
        Type("ps_partkey", INT, 0, NULL, false), 
        Type("ps_suppkey", INT, 0, NULL, false), 
        Type("ps_availqty", INT),
        Type("ps_supplycost", DECIMAL), 
        Type("ps_comment", VARCHAR, 199),
    });
    tablePointer->createPrimaryKey(new PrimaryKey(tablePointer, 2, new int[2]{0, 1}));
    tablePointer->createForeignKey(new ForeignKey(tablePointer, 1, new int[1]{0}, "ps_partkey"), db->table[0]->p_key);
    // suppliers 不导入之后，这个也没有了
    // assert(tablePointer->createForeignKey(new ForeignKey(tablePointer, 1, new int[1]{1}, "ps_suppkey"), db->table[3]->p_key) == 0);
    importOneTable(tablePointer, "data/partsupp.tbl.csv", delimeter);

    // table: 5
    tablePointer = db->createTable("orders", 9, new Type[9]{
        Type("o_orderkey", INT, 0, NULL, false), 
        Type("o_custkey", INT, 0, NULL, false), 
        Type("o_orderstatus", CHAR, 1),
        Type("o_totalprice", DECIMAL), 
        Type("o_orderdate", DATE),
        Type("o_orderpriority", CHAR, 15),
        Type("o_clerk", CHAR, 15),
        Type("o_shippriority", INT),
        Type("o_comment", VARCHAR, 79),
    });
    tablePointer->createPrimaryKey(new PrimaryKey(tablePointer, 1, new int[1]{0}, "o_orderkey"));
    // 不导入 supply
    // assert(tablePointer->createForeignKey(new ForeignKey(table, 1, new int[1]{1}, "o_custkey\0"), db->table[4]->p_key) == 0);
    tablePointer->createForeignKey(new ForeignKey(tablePointer, 1, new int[1]{1}, "o_custkey"), db->table[3]->p_key);
    importOneTable(tablePointer, "data/orders.tbl.csv", delimeter);

    delete db;
}

