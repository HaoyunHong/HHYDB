# pragma once

#include "Const.h"
#include "Database.h"
#include "Table.h"
#include "Something.h"
#include "Type.h"
using namespace std;

#define filterTable(it) ((it) < 0 ? currentDB->tempTable[-1-it] : currentDB->table[it])
#define int2uint2Col(it) (filterTable((it).first)->col_ty[(it).second])

struct YYType_Aggr {
	enumAggr ty;
	str2str col;
};

struct YYType_Where {
	uint ty;
	vector<str2str> cols;
	enumOp op;
	vector<Something> rightV;
	vector<str2str> rightV_cols;
	int rightV_table;
};

struct YYType {
	string S;
	int I;
	uint32_t U;
	long double D;
	Type TY;
	vector<Type> V_TY;
	Something V;
	vector<Something> V_V;
	vector<string> V_S;
	str2str P_SS;
	vector<str2str> V_P_SS;
	int2str P_IS;
	vector<int2str> V_P_IS;
	enumOp eOP;
	YYType_Where W;
	vector<YYType_Where> V_W;
	vector<str2something> V_P_SA;
	YYType_Aggr G;
	vector<YYType_Aggr> V_G;
};

#define YYSTYPE YYType
