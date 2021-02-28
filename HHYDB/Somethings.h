# pragma once

#include "Something.h"

class Somethings {
    
public:
    std::vector<Something> v;
    Somethings() {}

    Somethings(Something* data, uint size) { 
        for (uint i = 0; i < size; i++) v.push_back(data[i]);
    }

    uint size() const { return v.size(); }

    void push_back(Something a) { v.push_back(a); }

    Something& operator[](uint idx) { return v[idx]; }

    void concatenate(Somethings b) {
        uint sz = b.size();
        for (uint i = 0; i < sz; i++) v.push_back(b[i]);
    }

    void clear() { v.clear(); }

    bool operator==(const Somethings &b) const {
        uint sz = size();
        if (sz != b.size()) return false;
        for (uint i = 0; i < sz; i++) if (!(v[i] == b.v[i])) return false;
        return true;
    }

    bool operator<(const Somethings &b) const {
        uint sz = size(), _sz = b.size();
        if (sz != _sz) return sz < _sz;
        for (uint i = 0; i < sz; i++) if (!(v[i] == b.v[i])) return v[i] < b.v[i];
        return false;
    }

    bool operator>(const Somethings &b) const {
        uint sz = size(), _sz = b.size();
        if (sz != _sz) return sz > _sz;
        for (uint i = 0; i < sz; i++) if (!(v[i] == b.v[i])) return v[i] > b.v[i];
        return false;
    }

    bool operator!=(const Somethings &b) const { return !(*this == b); }

    bool operator<=(const Somethings &b) const { return !(*this > b); }

    bool operator>=(const Somethings &b) const { return !(*this < b); }
};