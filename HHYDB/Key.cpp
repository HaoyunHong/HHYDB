#include "Key.h"
#include "Database.h"
#include "Table.h"

ForeignKey::ForeignKey(Table* s, json j, Database* db) : Key(s, j["data"]) {
    p = db->table[j["sid"].get<int>()]->p_key;
    p->f.push_back(this);
}

json ForeignKey::toJson() {
    json j;
    j["data"] = Key::toJson();
    if(p==nullptr){
        printf("No primary key in table %s now!\n", table->name);
    }
    else{
        j["sid"] = p->table->table_id;
    }
    return j;
}