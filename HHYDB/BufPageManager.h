# pragma once

#include "Const.h"
#include "FileManager.h"
#include "Hash.h"
#include "MemManager.h"

class Hash;
class MemManager;
class FileManager;

class BufPageManager {
private:
    FileManager* fm;
    BufType addr[65536];
    int file_id[65536];
    int page_id[65536];
    bool dirty[65536];
    Hash* hash;
    MemManager* pool;
    
    int lastFileID, lastPageID, lastIndex;
    BufType lastBuf;

    BufType fetchPage(int fileID, int pageID, int& index);
    void writeBack(int index); // writeBack will throw the buf away
    
public:
    BufPageManager(FileManager* fm);
    ~BufPageManager();
    BufType getPage(int fileID, int pageID);
    BufType getPage(int fileID, int pageID, int& index);
    void markDirty(int index);
    bool check(int fileID, int pageID, int index);
    void closeFile(int fileID);
};