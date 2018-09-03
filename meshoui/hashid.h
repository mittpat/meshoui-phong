#pragma once

#include <string>

struct HashId final
{
    static size_t makeHash(const std::string & str);
    HashId();
    HashId(const std::string & str);
    HashId(const char * str);
    bool operator==(const HashId& rhs) const;
    bool operator!=(const HashId& rhs) const;
    size_t id;
    std::string str;
    operator size_t() const;
    operator bool() const;
};
inline size_t HashId::makeHash(const std::string & str)
{
    std::hash<std::string> hasher;
    return hasher(str);
}
inline HashId::HashId() : id(0) {}
inline HashId::HashId(const std::string & s) : id(makeHash(s)), str(s) {}
inline HashId::HashId(const char * s) : id(makeHash(s)), str(s) {}
inline bool HashId::operator==(const HashId& rhs) const { return rhs.id == id; }
inline bool HashId::operator!=(const HashId& rhs) const { return rhs.id != id; }
inline HashId::operator size_t() const { return id; }
inline HashId::operator bool() const { return id != 0; }
