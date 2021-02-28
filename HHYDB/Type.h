# pragma once

#include "Const.h"
#include "Something.h"

using json = nlohmann::json;

class Type {
    bool canNull = true;

public:
    Type(const char* _name = "", dataType _ty = INT, unsigned char _char_len = 0, Something _sth = Something(), bool _canNull = true) {
        canNull = _canNull;
        ty = _ty;
        char_len = _char_len;
        sth = _sth;
        strcpy(name, _name);
        if(_ty == CHAR || _ty == VARCHAR){
            int len = 0;
            for (int i = 0; name[i] != '\0'; i++){
                len ++;
            }
            for (int i = 0; i < len; i++){
                // 都变成小写
                if (name[i]>=65 && name[i] <= 90)
                {
                    name[i] += 32;
                }	
            }
        }
    }    

    Type(json jsonList);

    bool canBeNull();
    void setNull(bool _canNull);
    uint size();
    json toJson();
    int printLen();

    Something sth;
    char name[MAX_NAME_LEN];
    dataType ty = INT;
    unsigned char char_len = 0;
    KeyType key = Plain;
    
};