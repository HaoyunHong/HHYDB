# pragma once

#include "Const.h"

class BaseHolder
{
public:
    BaseHolder() {}
    virtual ~BaseHolder() {}
    virtual BaseHolder *clone() = 0;
};

template <typename T>
class Holder : public BaseHolder
{
public:
    Holder(const T &t)
        : _value(t)
    {
    }
    ~Holder() {}
    BaseHolder *clone() override
    {
        return new Holder<T>(_value);
    }

public:
    T _value;
};

class Something
{
public:
    template <typename ValueType>
    Something(const ValueType &value)
    {
        _pValue = new Holder<ValueType>(value);
    }

    Something() :_pValue(nullptr) {}

    Something(const Something &any) {
        if (any._pValue)
            _pValue = any._pValue->clone();
        else 
            _pValue = 0;
    }

    ~Something() {
        if (_pValue)
            delete _pValue;
    }

    Something& operator=(const Something& any) {
        _pValue = nullptr;
        if (any._pValue) {
            _pValue = any._pValue->clone();
        }
        return *this;
    }

    template <typename ValueType>
    Something &operator=(const ValueType &value)
    {
        Something tmp(value);
        if (_pValue){
            delete _pValue;
        }
        _pValue = tmp._pValue;
        tmp._pValue = nullptr;
        return *this;
    }

    // 把原本的转换了
    template <typename ValueType>
    ValueType *anyCast() const
    {
        Holder<ValueType> *p = dynamic_cast<Holder<ValueType> *>(_pValue);
        return p ? &p->_value : nullptr;
    }

    // 返回原本的转换后的副本
    template <typename ValueType>
    ValueType &anyRefCast()
    {
        return (dynamic_cast<Holder<ValueType> &>(*_pValue))._value;
    }

    bool operator<(const Something &b) const
    {
        if (isNull() || b.isNull()) {
            if (isNull() && b.isNull()) return false;
            if (isNull() && !b.isNull()) return true;
            if (!isNull() && b.isNull()) return false;
        } else if (b.anyCast<char*>() != nullptr) {
            return *this < (const char*)*b.anyCast<char*>();
        } else if (b.anyCast<int>() != nullptr) {
            return *this < *b.anyCast<int>();
        } else if (b.anyCast<long double>() != nullptr) {
            return *this < *b.anyCast<long double>();
        } else if (b.anyCast<uint32_t>() != nullptr) {
            return *this < *b.anyCast<uint32_t>();
        }
        return false;
    }

    bool operator>(const Something &b) const
    {
        if (isNull() || b.isNull()) {
            if (isNull() && b.isNull()) return false;
            if (isNull() && !b.isNull()) return false;
            if (!isNull() && b.isNull()) return true;
        } else if (b.anyCast<char*>() != nullptr) {
            return *this > (const char*)*b.anyCast<char*>();
        } else if (b.anyCast<int>() != nullptr) {
            return *this > *b.anyCast<int>();
        } else if (b.anyCast<long double>() != nullptr) {
            return *this > *b.anyCast<long double>();
        } else if (b.anyCast<uint32_t>() != nullptr) {
            return *this > *b.anyCast<uint32_t>();
        }
        return false;
    }

    bool operator==(const Something &b) const
    {
        if (isNull() || b.isNull()) {
            if (isNull() && b.isNull()) return true;
            if (isNull() && !b.isNull()) return false;
            if (!isNull() && b.isNull()) return false;
        } else if (b.anyCast<char*>() != nullptr) {
            return *this == (const char*)*b.anyCast<char*>();
        } else if (b.anyCast<int>() != nullptr) {
            return *this == *b.anyCast<int>();
        } else if (b.anyCast<long double>() != nullptr) {
            return *this == *b.anyCast<long double>();
        } else if (b.anyCast<uint32_t>() != nullptr) {
            return *this == *b.anyCast<uint32_t>();
        }
        return false;
    }

    bool operator<=(const Something &b) const { return !(*this > b); }

    bool operator>=(const Something &b) const { return !(*this < b); }

    bool operator!=(const Something &b) const { return !(*this == b); }

    bool operator<(const char* value) const
    {
        if (isNull()) {
            return true; 
        } else if (this->anyCast<char*>() != nullptr) {
            return strcmp(*this->anyCast<char*>(), value) < 0;
        }
        return false;
    }

    bool operator>(const char* value) const
    {
        if (isNull()) {
            return false; 
        } else if (this->anyCast<char*>() != nullptr) {
            return strcmp(*this->anyCast<char*>(), value) > 0;
        }
        return false;
    }

    bool operator==(const char* value) const
    {
        if (isNull()) {
            return false; 
        } else if (this->anyCast<char*>() != nullptr) {
            return strcmp(*this->anyCast<char*>(), value) == 0;
        }
        return false;
    }

    bool operator<=(const char* value) const { return !(*this > value); }

    bool operator>=(const char* value) const { return !(*this < value); }

    bool operator!=(const char* value) const { return !(*this == value); }

    template <typename ValueType>
    bool operator<(const ValueType &value) const
    {
        if (isNull()) {
            return true; 
        } else if (this->anyCast<ValueType>() != nullptr) {
            return *this->anyCast<ValueType>() < value;
        }
        return false;
    }

    template <typename ValueType>
    bool operator>(const ValueType &value) const
    {
        if (isNull()) {
            return false; 
        } else if (this->anyCast<ValueType>() != nullptr) {
            return *this->anyCast<ValueType>() > value;
        }
        return false;
    }

    template <typename ValueType>
    bool operator==(const ValueType &value) const
    {
        if (isNull()) {
            return false; 
        } else if (this->anyCast<ValueType>() != nullptr) {
            return *this->anyCast<ValueType>() == value;
        }
        return false;
    }

    template <typename ValueType>
    bool operator<=(const ValueType &value) const { return !(*this > value); }

    template <typename ValueType>
    bool operator>=(const ValueType &value) const { return !(*this < value); }

    template <typename ValueType>
    bool operator!=(const ValueType &value) const { return !(*this == value); }

    bool isNull() const { return _pValue == nullptr; }

private:
    BaseHolder *_pValue;
};