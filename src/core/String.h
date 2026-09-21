// Reconstructed from Grimrock.bin.x86 String.cpp (0x080c5a00-0x080c6a20).
// Layout {char* m_pData; int m_size; int m_capacity}. m_pData may be null for an
// empty string; c_str() then returns "". Buffer is new[]'ed with capacity + 1.
#pragma once
#include "core/Array.h"
#include <cstring>

namespace core
{

class String
{
  public:
    String() : m_pData(0), m_size(0), m_capacity(0) {}
    String(const char* s) : m_pData(0), m_size(0), m_capacity(0)
    {
        push_back(s, (int)strlen(s));
    }
    String(const char* s, int n) : m_pData(0), m_size(0), m_capacity(0)
    {
        push_back(s, n);
    }
    String(const String& other) : m_pData(0), m_size(0), m_capacity(0)
    {
        push_back(other.c_str(), other.m_size);
    }
    ~String()
    {
        if (m_pData)
            delete[] m_pData;
    }
    // 0x080e7760 core::String::operator=(char const*)
    String& operator=(const char* s)
    {
        char* old = m_pData;
        m_size = 0;
        m_capacity = 0;
        m_pData = 0;
        push_back(s, (int)strlen(s));
        if (old)
            delete[] old;
        return *this;
    }
    String& operator=(const String& other)
    {
        if (this != &other)
        {
            char* old = m_pData;
            m_size = 0;
            m_capacity = 0;
            m_pData = 0;
            push_back(other.c_str(), other.m_size);
            if (old)
                delete[] old;
        }
        return *this;
    }

    const char* c_str() const
    {
        return m_pData ? m_pData : "";
    }
    int size() const
    {
        return m_size;
    }
    int length() const
    {
        return m_size;
    }
    bool empty() const
    {
        return m_size == 0;
    }
    char operator[](int i) const
    {
        return m_pData[i];
    }
    char& operator[](int i)
    {
        return m_pData[i];
    }
    void clear()
    {
        m_size = 0;
        if (m_pData)
            m_pData[0] = 0;
    }

    void reserve(int n);
    void push_back(const char* s, int n);
    void push_back(char c)
    {
        push_back(&c, 1);
    }
    void append(const char* s);
    void append(const String& s);
    void insert(int pos, const char* s);
    void insert(int pos, const String& s);
    void erase(int pos);
    void erase(int first, int last);
    int find(const char* s, int start = 0) const;
    int find(const String& s, int start = 0) const;
    int find_last(const char* s, int start = 0) const;
    int find_last(const String& s, int start = 0) const;
    int count(const char* s, int start = 0) const;
    int count(const String& s, int start = 0) const;
    void replace(const char* from, const char* to);
    void replace(const String& from, const String& to);
    String substr(int start, int end = -1) const;
    bool startsWith(const char* s) const;
    bool startsWith(const String& s) const;
    bool endsWith(const char* s) const
    {
        int n = (int)strlen(s);
        return n <= m_size && strcmp(c_str() + m_size - n, s) == 0;
    }
    void tolower();
    void toupper();

    String& operator+=(const char* s)
    {
        append(s);
        return *this;
    }
    String& operator+=(const String& s)
    {
        append(s);
        return *this;
    }
    String& operator+=(char c)
    {
        push_back(&c, 1);
        return *this;
    }

  private:
    char* m_pData;
    int m_size;
    int m_capacity;
};

inline bool operator==(const String& a, const String& b)
{
    return strcmp(a.c_str(), b.c_str()) == 0;
}
inline bool operator==(const String& a, const char* b)
{
    return strcmp(a.c_str(), b) == 0;
}
inline bool operator==(const char* a, const String& b)
{
    return strcmp(a, b.c_str()) == 0;
}
inline bool operator!=(const String& a, const String& b)
{
    return !(a == b);
}
inline bool operator!=(const String& a, const char* b)
{
    return !(a == b);
}
inline bool operator<(const String& a, const String& b)
{
    return strcmp(a.c_str(), b.c_str()) < 0;
}
inline String operator+(const String& a, const String& b)
{
    String r(a);
    r.append(b);
    return r;
}
inline String operator+(const String& a, const char* b)
{
    String r(a);
    r.append(b);
    return r;
}
inline String operator+(const char* a, const String& b)
{
    String r(a);
    r.append(b);
    return r;
}

// Math.cpp: 0x080c61c0, 0x080c6610
Array<String> split(const char* s, const char* separator);
String formatString(const char* format, ...);

// 32-bit FNV-1a parameters, as used by ArchiveFileSystem, HashMap and sys.fnv32.
constexpr unsigned int Fnv1a32Basis = 0x811c9dc5u;
constexpr unsigned int Fnv1a32Prime = 0x01000193u;

inline unsigned int hashString(const char* str)
{
    unsigned int hash = Fnv1a32Basis;
    for (; *str; ++str)
        hash = (hash ^ (unsigned char)*str) * Fnv1a32Prime;
    return hash;
}

} // namespace core
