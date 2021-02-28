#include "Print.h"

using namespace std;

// lenLimit其实可以改一下，就是不是整个的长度，而是用\0之前的长度，可以把长度打出来看看
string lenLimit(string str, uint len) {
    // 这里补两个还能让格式化输出更好看
    if (str.length() > len) {
        return str.substr(0, len-2) + ".."; 
    }
    else {
        return str + string(len - str.length(), ' ');
    }
}

string Type2Str(dataType ty) {
    switch (ty) {
    case INT: return "INT";
    case CHAR: return "CHAR";
    case VARCHAR: return "VARCHAR";
    case DATE: return "DATE";
    case DECIMAL: return "DECIMAL";
    default: return "";
    }
}

string Aggr2Str(enumAggr ty) {
    switch (ty) {
    case AG_COUNT: return "COUNT()";
    case AG_MIN: return "MIN()";
    case AG_MAX: return "MAX()";
    case AG_SUM: return "SUM()";
    case AG_AVG: return "AVG()";
    default: return "";
    }
}

string Any2Str(Something val) {
    if (val.anyCast<int>() != NULL) {
        return to_string(*val.anyCast<int>());
    } else if (val.anyCast<char*>() != NULL) {
        return string(*val.anyCast<char*>());
    } else if (val.anyCast<uint32_t>() != NULL) {
        string str = to_string(*val.anyCast<uint32_t>());
        return str.substr(0, 4) + "-" + str.substr(4, 2) + "-" + str.substr(6, 2);
    } else if (val.anyCast<long double>() != NULL) {
        return to_string(*val.anyCast<long double>());
    } else return "NULL";
}

vector<pair<string, int> > Print::t;
int Print::col_num;

void Print::title(vector<pair<string, int> > v) {
    t = v;
    col_num = v.size();

    string str = "+";
    for (int i = 0; i < col_num; i++) str += string(2 + t[i].second, '-') + '+';
    cout << str << endl;

    str = "|";
    for (int i = 0; i < col_num; i++) {

        str += ' ' + lenLimit(t[i].first, t[i].second) + " |";
    }
    cout << str << endl;

    str = "+";
    for (int i = 0; i < col_num; i++) str += string(2 + t[i].second, '-') + '+';
    cout << str << endl;
}

void Print::row(Somethings v) {
    string str = "|";
    for (int i = 0; i < col_num; i++) str += ' ' + lenLimit(Any2Str(v[i]), t[i].second) + " |";
    cout << str << endl;
}

void Print::end() {
    string str = "+";
    for (int i = 0; i < col_num; i++) str += string(2 + t[i].second, '-') + '+';
    cout << str << endl;
    t.clear();
    col_num = 0;
}