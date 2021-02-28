#include "Type.h"

Type:: Type(json jsonList) {
    strcpy(name, jsonList["name"].get<std::string>().c_str());
    ty = (dataType)jsonList["ty"].get<int>();
    char_len = jsonList["char_len"].get<int>();
    key = (KeyType)jsonList["key"].get<int>();
    canNull = jsonList["null"].get<bool>();
    if (jsonList.count("sth")) {
        switch (ty) {
        case INT: { sth = jsonList["sth"].get<int>(); break; }
        case DATE: { sth = jsonList["sth"].get<int>(); break; }
        case CHAR: { sth = jsonList["sth"].get<std::string>().c_str(); break; }
        case VARCHAR: { sth = jsonList["sth"].get<std::string>().c_str(); break; }
        case DECIMAL: { sth = jsonList["sth"].get<long double>(); break; }
        }
    } else {
        sth = Something();
    }
}


bool Type::canBeNull() 
{ 
    return canNull; 
}

void Type::setNull(bool _canNull) 
{ 
    canNull = _canNull; 
}

uint Type::size() {
    switch (ty) {
    case INT: return 4;
    case CHAR: return char_len;
    case VARCHAR: return 8;
    case DATE: return 4;
    case DECIMAL: return 16;
    default: return -1;
    }
}

json Type::toJson() {
    json jsonList;
    jsonList["name"] = name;
    jsonList["ty"] = ty;
    jsonList["char_len"] = char_len;
    jsonList["key"] = key;
    jsonList["null"] = canNull;
    if (sth.anyCast<int>() != nullptr) {
        jsonList["sth"] = sth.anyRefCast<int>();
    } else if (sth.anyCast<char*>() != nullptr) {
        jsonList["sth"] = sth.anyRefCast<char*>();
    }
    return jsonList;
}

int Type::printLen() {
    switch (ty) {
    case INT: return 10;
    case CHAR: return std::min(char_len, (unsigned char)20);
    case VARCHAR: return std::min(char_len, (unsigned char)20);
    case DATE: return 10;
    case DECIMAL: return 10;
    default: return -1;
    }
}