# pragma once

#include<vector>
#include <stdint.h>
#include <string.h>
#include <string>
#include <math.h>
#include <iostream>
#include "json.hpp"
#include <stdio.h>
#include <map>
#include <time.h>
#include <regex>
#include <dirent.h>
#include <fcntl.h>
#include <termio.h>

using namespace std;

class Something;

typedef unsigned int uint;
typedef unsigned char* BufType;

// 键值对
typedef pair<string, string> str2str;
typedef pair<int, string> int2str;
typedef pair<int, uint> int2uint;

typedef pair<string, Something> str2something;
typedef pair<int, Something> int2something;

enum enumOp {
    OP_EQ,
    OP_NEQ,
    OP_LE,
    OP_GE,
    OP_LS, 
    OP_GT,
    OP_NL, 
    OP_NNL, 
    OP_IN, 
    OP_LIKE,
};

enum enumAggr {
    AG_COUNT,
    AG_MIN,
    AG_MAX,
    AG_SUM,
    AG_AVG,
};

struct AggrStatement {
    enumAggr ty;
    int2uint col;
};

struct WhereStatement {
    vector<int2uint> cols;
    enumOp op;
    bool any = false;
    bool all = false;
    uint rightV_ty = 0;
    vector<Something> rightV;
    vector<int2uint> rightV_cols;
    int rightV_table;
};

struct SelectStatement {
    bool build = false;
    vector<int2uint> select;
    vector<AggrStatement> aggr;
    vector<int2str> from; 
    vector<WhereStatement> where;
    vector<int> recursion;
};

enum dataType {
    INT,
    CHAR,
    VARCHAR,
    DATE,
    DECIMAL,
};

enum KeyType {
    Primary,
    Foreign,
    Plain,
};

inline char getch() {
    struct termios tm, tm_old;
    int fd = 0, ch;
    if (tcgetattr(fd, &tm) < 0) return -1;
    tm_old = tm;
    cfmakeraw(&tm);
    if (tcsetattr(fd, TCSANOW, &tm) < 0) return -1;
    ch = getchar();
    if (tcsetattr(fd, TCSANOW, &tm_old) < 0) return -1;
    return ch;
}

#define PAGE_SIZE 8192
#define MAX_NAME_LEN 64
#define MAX_FILE_NUM 128
