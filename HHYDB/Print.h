# pragma once

#include "Const.h"
#include "Something.h"
#include "Somethings.h"
#include "Type.h"
#include "Database.h"

using namespace std;

string Aggr2Str(enumAggr ty);
string Any2Str(Something val);

class Print {
public:
    static vector<pair<string, int>> t;
    static int col_num;
    static void title(vector<pair<string, int>> v);
    static void row(Somethings v);
    static void end();
};

