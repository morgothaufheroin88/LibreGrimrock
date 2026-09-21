// Reconstructed from Grimrock.bin.x86 String.cpp / Math.cpp.
#include "core/String.h"
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace core
{

// 0x080c5bb0
void String::reserve(int n)
{
    int cap = m_capacity;
    if (cap < n)
    {
        do
        {
            cap = (cap * 3) / 2;
            if (cap < n)
                cap = n;
        } while (cap < n);
        m_capacity = cap;
        char* data = new char[cap + 1];
        if (m_pData)
            memcpy(data, m_pData, m_size);
        data[m_size] = 0;
        if (m_pData)
            delete[] m_pData;
        m_pData = data;
    }
}

// 0x080c5ed0
void String::push_back(const char* s, int n)
{
    reserve(m_size + n);
    if (n > 0)
        memcpy(m_pData + m_size, s, n);
    m_size += n;
    if (m_pData)
        m_pData[m_size] = 0;
}

// 0x080c6730
void String::append(const char* s)
{
    int n = (int)strlen(s);
    reserve(m_size + n);
    memcpy(m_pData + m_size, s, n);
    m_size += n;
    if (m_pData)
        m_pData[m_size] = 0;
}

// 0x080c6420
void String::append(const String& s)
{
    push_back(s.c_str(), s.m_size);
}

// 0x080c67f0
void String::insert(int pos, const char* s)
{
    if (pos < 0 || pos > m_size || *s == 0)
        return;
    int n = (int)strlen(s);
    reserve(m_size + n);
    memmove(m_pData + pos + n, m_pData + pos, m_size - pos + 1);
    memcpy(m_pData + pos, s, n);
    m_size += n;
}

// 0x080c6900
void String::insert(int pos, const String& s)
{
    insert(pos, s.c_str());
}

// 0x080c5e90
void String::erase(int pos)
{
    erase(pos, pos);
}

// 0x080c5b40
void String::erase(int first, int last)
{
    if (first >= 0 && last >= 0 && first < m_size && first <= last && last < m_size)
    {
        memmove(m_pData + first, m_pData + last + 1, m_size - last);
        m_size -= last - first + 1;
    }
}

// 0x080c5a30
int String::find(const char* s, int start) const
{
    if (start < 0 || start >= m_size)
        return -1;
    const char* match = strstr(m_pData + start, s);
    return match ? (int)(match - m_pData) : -1;
}

// 0x080c5dd0
int String::find(const String& s, int start) const
{
    return find(s.c_str(), start);
}

// 0x080c5cf0
int String::find_last(const char* s, int start) const
{
    if (start < 0 || start >= m_size)
        return -1;
    int result = -1;
    for (;;)
    {
        const char* match = strstr(m_pData + start, s);
        if (!match)
            break;
        int i = (int)(match - m_pData);
        if (i < 0)
            break;
        result = i;
        start = i + 1;
        if (start >= m_size)
            break;
    }
    return result;
}

// 0x080c5d50
int String::find_last(const String& s, int start) const
{
    return find_last(s.c_str(), start);
}

// 0x080c5c90
int String::count(const char* s, int start) const
{
    if (start < 0 || start >= m_size)
        return 0;
    int n = 0;
    for (;;)
    {
        const char* match = strstr(m_pData + start, s);
        if (!match)
            break;
        int i = (int)(match - m_pData);
        if (i < 0)
            break;
        ++n;
        start = i + 1;
        if (start >= m_size)
            break;
    }
    return n;
}

// 0x080c5c30
int String::count(const String& s, int start) const
{
    return count(s.c_str(), start);
}

// 0x080c5f80
void String::replace(const char* from, const char* to)
{
    int fromLen = (int)strlen(from);
    if (fromLen == 0)
        return;
    if (fromLen == 1 && strlen(to) == 1)
    {
        for (int i = 0; i < m_size; ++i)
            if (m_pData[i] == *from)
                m_pData[i] = *to;
        return;
    }
    while (m_size > 0)
    {
        const char* match = strstr(m_pData, from);
        if (!match)
            return;
        int pos = (int)(match - m_pData);
        if (pos < 0)
            return;
        erase(pos, pos + fromLen - 1);
        if (m_size < pos || *to == 0)
            return;
        int toLen = (int)strlen(to);
        reserve(m_size + toLen);
        memmove(m_pData + pos + toLen, m_pData + pos, m_size - pos + 1);
        memcpy(m_pData + pos, to, toLen);
        m_size += toLen;
    }
}

// 0x080c6190
void String::replace(const String& from, const String& to)
{
    replace(from.c_str(), to.c_str());
}

// 0x080c64e0
String String::substr(int start, int end) const
{
    if (start < 0)
        start = m_size;
    else if (start > m_size)
        start = m_size;
    int stop = end < 0 ? m_size : end + 1;
    if (stop > m_size)
        stop = m_size;
    int n = stop - start;
    String result;
    if (n > 0)
        result.reserve(n);
    result.push_back(c_str() + start, n > 0 ? n : 0);
    return result;
}

// 0x080c5a80
bool String::startsWith(const char* s) const
{
    return strncmp(c_str(), s, strlen(s)) == 0;
}

// 0x080c5e30
bool String::startsWith(const String& s) const
{
    return startsWith(s.c_str());
}

// 0x080c5b00
void String::tolower()
{
    for (int i = 0; i < m_size; ++i)
        m_pData[i] = (char)::tolower(m_pData[i]);
}

// 0x080c5ac0
void String::toupper()
{
    for (int i = 0; i < m_size; ++i)
        m_pData[i] = (char)::toupper(m_pData[i]);
}

// 0x080c61c0 core::split(char const*, char const*)
Array<String> split(const char* s, const char* separator)
{
    Array<String> result;
    int len = (int)strlen(s);
    int start = 0;
    while (start < len)
    {
        const char* match = strstr(s + start, separator);
        int end = match ? (int)(match - s) : len;
        String token(s + start, end - start > 0 ? end - start : 0);
        result.push_back(token);
        start = end + 1;
    }
    return result;
}

// 0x080c6610 core::formatString(char const*, ...)
String formatString(const char* format, ...)
{
    char buffer[1036];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return String(buffer);
}

} // namespace core
