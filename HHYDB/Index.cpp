#include "Index.h"

#include "Table.h"


using namespace std;

extern char* dirPath(const char* dir, const char* filename, const char* subname, const char* filetype);

json Index::toJson() {
    json j;
    j["name"] = name;
    j["key_num"] = key_num;
    for (uint i = 0; i < key_num; i++) j["key"].push_back(key[i]);
    for (uint i = 0; i < key_num; i++) j["offset"].push_back(offset[i]);
    j["page_num"] = page_num;
    j["root_page"] = root_page;
    j["next_del_page"] = next_del_page;
    j["record_size"] = record_size;
    for (uint i = 0; i < key_num; i++) j["ty"].push_back(ty[i]);
    return j;
}

Index::Index(Table* table, json j) : table(table) {
    strcpy(name, j["name"].get<std::string>().c_str());
    key.clear();
    ty.clear();
    offset.clear();
    key_num = j["key_num"].get<int>();
    for (uint i = 0; i < key_num; i++) key.push_back(j["key"][i]);
    for (uint i = 0; i < key_num; i++) offset.push_back(j["offset"][i]);
    page_num = j["page_num"].get<int>();
    root_page = j["root_page"].get<int>();
    next_del_page = j["next_del_page"].get<int>();
    record_size = j["record_size"].get<int>();
    for (uint i = 0; i < key_num; i++) ty.push_back((dataType)j["ty"][i]);
    fileID = -1;
}

Index::Index(Table* table, const char* name, vector<uint> key, int btree_max_per_node) : table(table), key(key), btree_max_per_node(btree_max_per_node) {
    strcpy(this->name, name);
    table->fileManager->createFile(dirPath(table->db->name, table->name, name, "usid"));
    page_num = 1;
    next_del_page = -1;
    record_size = 8;
    this->key.clear();
    this->offset.clear();
    this->ty.clear();
    this->key_num = key.size();
    for (auto i : key) {
        this->key.push_back(i);
        this->ty.push_back(table->col_ty[i].ty);
    }
    for (uint i = 0; i < key.size(); i++) {
        this->offset.push_back(i == 0 ? 0 : this->offset[i - 1]);
        this->offset[i] += table->col_ty[key[i]].size();
    }
    for (uint i = 0; i < key.size(); i++) this->offset[i] -= table->col_ty[key[i]].size();
    for (uint i = 0; i < key.size(); i++)
        record_size += table->col_ty[key[i]].size();
    max_recond_num = (PAGE_SIZE - 18) / record_size;

    open();

    int index;
    BufType buf = table->bufManager->getPage(fileID, 0, index);
    *(uint32_t *)(buf+0) = -1;
    *(uint32_t *)(buf+4) = -1;
    *(uint32_t *)(buf+8) = -1;
    *(uint32_t *)(buf+12) = -1;
    *(uint16_t *)(buf+16) = 0;
    table->bufManager->markDirty(index);
    
    root_page = 0;
    close();
}

void Index::open() {
    table->fileManager->openFile(dirPath(table->db->name, table->name, name, "usid"), fileID);
}

void Index::close() {
    table->fileManager->closeFile(fileID);
    fileID = -1;
}

void Index::remove() {
    if (fileID != -1) {
        close();
    }
    table->fileManager->deleteFile(dirPath(table->db->name, table->name, name, "usid"));
}

void Index::Btree_remove(BNode* node) {
    if (node == nullptr) return;
    Btree_remove(node->leftPage);
    Btree_remove(node->rightPage);
    if (node->RCount) {
        for (int i = 0; i <= node->RCount; i++) {
            Btree_remove(node->childPointers[i]);
        }
    }
}

BNode* Index::convert_buf_to_BNode(int index) {
    BNode* ans = new BNode();
    ans->childs.clear();
    ans->childPointers.clear();
    ans->record.clear();
    ans->leftPage = nullptr;
    ans->rightPage = nullptr;
    
    ans->index = index;

    BufType buf = table->bufManager->getPage(fileID, index);

    ans->leftPID = *(uint32_t *)(buf + 4);
    ans->rightPID = *(uint32_t *)(buf + 8);
    ans->fatherID = *(uint32_t *)(buf + 12);
    ans->RCount = *(uint16_t *)(buf + 16);

    if (ans->RCount != 0) {
        ans->childs.push_back(*(uint32_t *)(buf + 18)); 
        if (ans->childs[0] != -1) ans->isLeaf = false;
    }
    else ans->childs.push_back(-1);

    for (int i = 0; i < ans->RCount; i++) {
        BRecord temp;
        temp.RID = *(uint32_t *)(buf + 22 + i * record_size);
        for (uint j = 0; j < this->key_num; j++) {
            if (this->ty[j] == dataType::INT) {
                bool flag = true;
                for (uint k = 0; k < 4; k++) {
                    if (*(BufType)(buf + 22 + i * record_size + 4 + this->offset[j] + k) != 255) {
                        flag = false;
                        break;
                    }
                }
                if(flag) temp.key.push_back(Something());
                else temp.key.push_back(Something(*(int*)(buf + 22 + i * record_size + 4 + this->offset[j])));
            }
            if (this->ty[j] == dataType::CHAR) {
                bool flag = true;
                for (uint k = 0; k < this->offset[j] - (j == 0 ? 0 : this->offset[j - 1]); k++) {
                    if (*(BufType)(buf + 22 + i * record_size + 4 + this->offset[j] + k) != 255) {
                        flag = false;
                        break;
                    }
                }
                if(flag){
                    char* str = new char[record_size - 8 + 1];
                    memcpy(str, buf + 22 + i * record_size + 4 + this->offset[j], this->offset[j] - (j == 0 ? 0 : this->offset[j - 1]));
                    str[record_size - 8] = '\0';
                    temp.key.push_back(Something(str));
                }
                else{
                    temp.key.push_back(Something());
                }
            }
        }
        ans->childs.push_back(*(uint32_t *)(buf + 22 + (i + 1) * record_size - 4));
        ans->record.push_back(temp);
    }
    return ans;
}

void Index::convert_BNode_to_buf(BNode* node) {
    int _index;
    BufType buf = table->bufManager->getPage(fileID, node->index, _index);

    *(uint32_t*) buf = -1; buf += 4; 
    *(uint32_t*) buf = node->leftPID; buf += 4; 
    *(uint32_t*) buf = node->rightPID; buf += 4; 
    *(uint32_t*) buf = node->fatherID; buf += 4;
    *(uint16_t*) buf = node->RCount; buf += 2; 

    if (node->RCount != 0) {
        *(uint32_t*) buf = node->childs[0]; buf += 4;
    }
    for (int i = 0; i < node->RCount; i++) {
        *(uint32_t*) buf = node->record[i].RID; buf += 4;
        for (uint j = 0; j < this->key_num; j++) {
            if (this->ty[j] == dataType::INT) {
                if(node->record[i].key[j].isNull()){
                    for (uint k = 0; k < 4; k++) {
                        *(BufType)buf = 255;
                        buf += 1;
                    }
                }
                else {
                    *(uint32_t*)buf = *node->record[i].key[j].anyCast<int>();
                    buf += 4;
                }
            }
            if (this->ty[j] == dataType::CHAR) {
                if(node->record[i].key[j].isNull()){
                    for (uint k = 0; k < this->offset[j] - (j == 0 ? 0 : this->offset[j - 1]); k++) {
                        *(BufType)buf = 255;
                        buf += 1;
                    }
                }
                else{
                    memcpy(buf, *node->record[i].key[j].anyCast<char*>(), this->offset[j] - (j == 0 ? 0 : this->offset[j - 1]));
                    buf += this->offset[j] - (j == 0 ? 0 : this->offset[j - 1]);
                }
            }
        }
        *(uint32_t*) buf = node->childs[i + 1]; buf += 4;
    }
    table->bufManager->markDirty(_index);
}

std::vector<int> Index::queryRecord(Somethings* info) {
    return Index::queryRecord(info, root_page);
}

std::vector<int> Index::queryRecord(Somethings* info, int now_index) {
    
    BNode* now = Index::convert_buf_to_BNode(now_index);

    std::vector<int> v;

    int l = -1, r = -1, i;
    for (i = 0; i < now->RCount; i++) {
        if (now->record[i].key == *info) {
            if (l == -1) {
                l = i;
            }
            r = i;
        }
        // 地址在后面的话
        else if (now->record[i].key > *info) {
            break;
        }
    }
    
    if (l == -1 && now -> isLeaf) 
    {
        return v;
    }
    else if (l == -1) {
        return Index::queryRecord(info, now->childs[i]);
    }

    for (int i = l; i <= r; i++) {
        v.push_back(now -> record[i].RID);
    }

    if (now -> isLeaf) {
        return v;
    }
    else {
        for (int i = l; i <= r + 1; i++) {
            std::vector<int> cv = Index::queryRecord(info, now->childs[i]);
            for (uint j = 0; j < cv.size(); j++) {
                v.push_back(cv[j]);
            }
        }
        return v;
    }
}

void Index::down(int now_index) {
    BNode* now = Index::convert_buf_to_BNode(now_index);

    if (now->fatherID == -1) {
        if (now->RCount != 0) 
            return;
        else {
            if (now->childs[0] == -1) return;
            root_page = now->childs[0];
            return;
        }
    }
    else if ((int)now->record.size() >= ceil(btree_max_per_node / 2.0) - 1) {
        return;
    }
    else {
        BNode* fa = Index::convert_buf_to_BNode(now->fatherID);
        uint i;
        for (i = 0; i < fa->childs.size(); i++) {
            if (fa->childs[i] == now->index) {
                break;
            }
        }
        if (i != 0) {
            BNode* left = Index::convert_buf_to_BNode(fa->childs[i - 1]);
            if (left->RCount <= ceil(btree_max_per_node / 2.0) - 1) {
                left->record.push_back(fa->record[i - 1]);
                for (int j = 0; j < now->RCount; j++) {
                    left->record.push_back(now->record[j]);
                    left->childs.push_back(now->childs[j]);
                    if(now->childs[j] != -1){
                        BNode* temp = Index::convert_buf_to_BNode(now->childs[j]);
                        temp->fatherID = left->index;
                        Index::convert_BNode_to_buf(temp);
                        delete temp;
                    }
                }
                left->RCount += 1 + now->RCount;
                left->childs.push_back(now->childs[now->childs.size() - 1]);
                vector<BRecord>::iterator it = fa->record.begin();
                vector<int>::iterator itx = fa->childs.begin();
                itx++;
                uint temp = i;
                while (--temp) {it ++; itx++; }
                fa->record.erase(it);
                fa->childs.erase(itx);
                fa->RCount--;
                Index::convert_BNode_to_buf(now);
                Index::convert_BNode_to_buf(left);       
                Index::convert_BNode_to_buf(fa);
                down(fa->index); 
            }
            else {
                now->record.insert(now->record.begin(), fa->record[i - 1]);
                now->childs.insert(now->childs.begin(), -1);
                now->RCount ++;

                fa->record[i - 1] = left->record[left->record.size() - 1];

                left->RCount --;
                left->record.erase(left->record.end() - 1);
                left->childs.erase(left->childs.end() - 1);
                Index::convert_BNode_to_buf(now);
                Index::convert_BNode_to_buf(left);
                Index::convert_BNode_to_buf(fa);
            }
        }
        else {
            BNode* right = Index::convert_buf_to_BNode(fa->childs[i + 1]);
            if (right->RCount <= ceil(btree_max_per_node / 2.0) - 1) {
                now->record.push_back(fa->record[i]);
                for (int j = 0; j < right->RCount; j++) {
                    now->record.push_back(right->record[j]);
                    now->childs.push_back(right->childs[j]);
                    if(right->childs[j] != -1){
                        BNode* temp = Index::convert_buf_to_BNode(right->childs[j]);
                        temp->fatherID = now->index;
                        Index::convert_BNode_to_buf(temp);
                        delete temp;
                    }
                }
                now->RCount += 1 + right->RCount;
                now->childs.push_back(right->childs[right->childs.size() - 1]);
                vector<BRecord>::iterator it = fa->record.begin();
                vector<int>::iterator itx = fa->childs.begin();
                uint temp = i;
                itx ++;
                while (temp--) {it ++; itx++; }
                fa->record.erase(it);
                fa->childs.erase(itx);
                fa->RCount--;
                Index::convert_BNode_to_buf(now);
                Index::convert_BNode_to_buf(right);        
                Index::convert_BNode_to_buf(fa);
                down(fa->index);
            }
            else {
                now->record.insert(now->record.begin(), fa->record[i - 1]);
                now->childs.insert(now->childs.begin(), -1);
                now->RCount ++;

                fa->record[i - 1] = right->record[0];

                right->RCount --;
                right->record.erase(right->record.begin());
                right->childs.erase(right->childs.begin());
                Index::convert_BNode_to_buf(now);
                Index::convert_BNode_to_buf(right);
                Index::convert_BNode_to_buf(fa);
            }
        }
    }
}

void Index::up(int now_index) {

    BNode* now = Index::convert_buf_to_BNode(now_index);
    if ((int)now->record.size() <= btree_max_per_node - 1) {
        return;
    }
    else {

        BNode* fa;

        if (now->fatherID == -1) {
            fa = new BNode();
            fa->index = page_num++;
            root_page = page_num - 1;
            fa->isLeaf = false;
        }
        else{
            fa = Index::convert_buf_to_BNode(now->fatherID);
        }
        BNode* right = new BNode();
        right->index = page_num++;

        BRecord mid = now->record[now->record.size() >> 1];

        uint loc = 0;
        for(uint i = 0;i < fa->childs.size();i ++){
            if(fa->childs[i] == now->index){
                loc = i;
                break;
            }
        }

        vector<BRecord>::iterator it = fa->record.begin() + loc;
        vector<int>::iterator itx = fa->childs.begin() + loc;

        fa->record.insert(it, mid);
        fa->childs.insert(itx, -1);
        fa->childs[loc] = now->index;
        fa->childs[loc + 1] = right->index;
        now->fatherID = fa->index;
        right->fatherID = fa->index;
        right->childs.clear();
        right->record.clear();
        right->childs.push_back(now->childs[(now->record.size() >> 1) + 1]);
        for (uint i = (now->record.size() >> 1) + 1; i < now->record.size(); i++) {
            right->childs.push_back(now->childs[i + 1]);
            right->record.push_back(now->record[i]);
            if(now->childs[i + 1] != -1){
                BNode* temp = Index::convert_buf_to_BNode(now->childs[i + 1]);
                temp->fatherID = right->index;
                Index::convert_BNode_to_buf(temp);
                delete temp;
            }
        }

        int left_size = (now->record.size() >> 1);
        int right_size = now->record.size() - left_size - 1;

        now->RCount = left_size;
        right->RCount = right_size;
        fa->RCount ++;

        Index::convert_BNode_to_buf(now);
        Index::convert_BNode_to_buf(right);
        Index::convert_BNode_to_buf(fa);
        up(fa->index);     
    }
}

void Index::insertRecord(Somethings* info, int RID) {
    return insertRecord(info, RID, root_page);
}

void Index::insertRecord(Somethings* info, int RID, int now_index) {

    BNode* now = Index::convert_buf_to_BNode(now_index);
    int i = 0;
    vector<BRecord>::iterator it;
    for (it = now->record.begin(); it!=now->record.end(); it++)
    {
        if (it->key > *info) {
            break;
        }
        if (it->key == *info && it->RID > RID) {
            break;
        }
        i ++;
    }

    if (now->isLeaf) {
        now->record.insert(it, BRecord(RID, *info));
        now->RCount++;
        now->childs.push_back(-1);
        Index::convert_BNode_to_buf(now);
        up(now->index);
        return;
    }
    else {
        Index::convert_BNode_to_buf(now);
        Index::insertRecord(info, RID, now->childs[i]);
    }
}

void Index::removeRecord(Somethings* info, int RID) {
    removeRecord(info, RID, root_page);
}

void Index::removeRecord(Somethings* info, int RID, int now_index) {
    BNode* now = Index::convert_buf_to_BNode(now_index);

    int i = 0;
    vector<BRecord>::iterator it;
    for (it = now->record.begin(); it!=now->record.end(); it++)
    {
        if (it->key > *info) {
            break;
        }
        if (it->key == *info && it->RID >= RID) {
            break;
        }
        i ++;
    }
    if (it != now->record.end() && it->RID == RID) {
        if (now->isLeaf) {
            now->record.erase(it);
            now->childs.pop_back();
            now->RCount--;
            Index::convert_BNode_to_buf(now);
            down(now->index);
        }
        else {
            BNode* follow_node = Index::convert_buf_to_BNode(now->childs[i + 1]);
            while (follow_node->childs[0] != -1)follow_node = Index::convert_buf_to_BNode(follow_node->childs[0]);
            now->record[i] = follow_node->record[0];
            Index::convert_BNode_to_buf(now);
            Index::convert_BNode_to_buf(follow_node);
            Index::removeRecord(&follow_node->record[0].key, follow_node->record[0].RID, follow_node->index);
        }
    }
    else {
        Index::convert_BNode_to_buf(now);
        if (!now->isLeaf) Index::removeRecord(info, RID, now->childs[i]);
    }
}

uint Index::queryRecordsNum(enumOp op, Somethings& data, int now_index){
    BNode* now = Index::convert_buf_to_BNode(now_index);

    uint ans = 0;
    std::vector<uint> temp;

    int i;
    for (i = 0; i < now->RCount; i++) {
        bool flag = false;
        switch(op){
            case OP_EQ:{
                if(now->record[i].key == data){
                    ans++;
                    temp.push_back(i);
                }
                else if(now->record[i].key > data){
                    flag = true;
                }
                break;
            }
            case OP_LIKE:{
                if(now->record[i].key == data){
                    ans++;
                    temp.push_back(i);
                }
                else if(now->record[i].key > data){
                    flag = true;
                }
                break;
            }
            case OP_NEQ:{
                if(now->record[i].key != data){
                    ans++;
                    temp.push_back(i);
                }
                break;
            }
            case OP_LE:{
                if(now->record[i].key <= data){
                    ans++;
                    temp.push_back(i);
                }
                else if(now->record[i].key > data){
                    flag = true;
                }
                break;
            }
            case OP_GE:{
                if(now->record[i].key >= data){
                    ans++;
                    temp.push_back(i);
                }
                break;
            }
            case OP_LS:{
                if(now->record[i].key < data){
                    ans++;
                    temp.push_back(i);
                }
                else if(now->record[i].key >= data){
                    flag = true;
                }
                break;
            }
            case OP_GT:{
                if(now->record[i].key > data){
                    ans++;
                    temp.push_back(i);
                }
                break;
            }
            case OP_NL:{
                bool ok = true;
                for(uint j = 0;j < now->record[i].key.size();j ++){
                    if(!now->record[i].key[j].isNull()){
                        ok = false;
                        break;
                    }
                }
                if(ok){
                    ans ++;
                    temp.push_back(i);
                }
                break;
            }
            case OP_NNL:{
                bool ok = false;
                for(uint j = 0;j < now->record[i].key.size();j ++){
                    if(!now->record[i].key[j].isNull()){
                        ok = true;
                        break;
                    }
                }
                if(ok){
                    ans ++;
                    temp.push_back(i);
                }
                break;
            }
            case OP_IN:{
                break;
            }
        }
        if(flag){
            break;
        }
    }

    switch(op){
        case OP_EQ:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_LIKE:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_NEQ:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
                if(i < temp.size() - 1 && temp[i] != temp[i + 1] - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_LE:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_GE:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_LS:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_GT:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_NL:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_NNL:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i]]);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    ans += queryRecordsNum(op,data,now->childs[temp[i] + 1]);
                }
            }
            break;
        }
        case OP_IN:{
            break;
        }
    }

    return ans;
}

std::vector<uint> Index::queryRecords(enumOp op, Somethings& data, int now_index){
    BNode* now = Index::convert_buf_to_BNode(now_index);

    std::vector<uint> ans;
    std::vector<uint> temp;

    int i;
    for (i = 0; i < now->RCount; i++) {
        bool flag = false;
        switch(op){
            case OP_EQ:{
                if(now->record[i].key == data){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                else if(now->record[i].key > data){
                    flag = true;
                }
                break;
            }
            case OP_NEQ:{
                if(now->record[i].key != data){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                break;
            }
            case OP_LE:{
                if(now->record[i].key <= data){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                else if(now->record[i].key > data){
                    flag = true;
                }
                break;
            }
            case OP_GE:{
                if(now->record[i].key >= data){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                break;
            }
            case OP_LS:{
                if(now->record[i].key < data){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                else if(now->record[i].key >= data){
                    flag = true;
                }
                break;
            }
            case OP_GT:{
                if(now->record[i].key > data){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                break;
            }
            case OP_NL:{
                bool ok = true;
                for(uint j = 0;j < now->record[i].key.size();j ++){
                    if(!now->record[i].key[j].isNull()){
                        ok = false;
                        break;
                    }
                }
                if(ok){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                break;
            }
            case OP_NNL:{
                bool ok = false;
                for(uint j = 0;j < now->record[i].key.size();j ++){
                    if(!now->record[i].key[j].isNull()){
                        ok = true;
                        break;
                    }
                }
                if(ok){
                    ans.push_back(now->record[i].RID);
                    temp.push_back(i);
                }
                break;
            }
            case OP_IN:{
                break;
            }
        }
        if(flag)break;
    }

    // 嵌套处理
    switch(op){
        case OP_EQ:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_LIKE:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_NEQ:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i < temp.size() - 1 && temp[i] != temp[i + 1] - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_LE:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_GE:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_LS:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_GT:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_NL:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_NNL:{
            for(uint i = 0;i < temp.size();i ++){
                if(now->childs[temp[i]] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i]]);
                    for (auto it: temp)ans.push_back(it);
                }
                if(i == temp.size() - 1 && now->childs[temp[i] + 1] != -1){
                    std::vector<uint> temp =  queryRecords(op,data,now->childs[temp[i] + 1]);
                    for (auto it: temp)ans.push_back(it);
                }
            }
            break;
        }
        case OP_IN:{
            break;
        }
    }

    return ans;
}

uint Index::queryRecordsNum(enumOp op, Somethings& data) {
    return queryRecordsNum(op,data,root_page);
}


vector<uint> Index::queryRecords(enumOp op, Somethings& data) {
    return queryRecords(op,data,root_page);
}
