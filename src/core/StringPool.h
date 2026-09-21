// core::StringPool: interned strings addressed by index (used for animation node names).
#pragma once
#include "core/Array.h"
#include "core/String.h"
#include <cstring>

namespace core
{

class StringPool
{
  public:
    ~StringPool() {}
    // Index of the string, adding it when new.
    int intern(const char* s)
    {
        for (int i = 0; i < m_strings.size(); ++i)
            if (strcmp(m_strings[i].c_str(), s) == 0)
                return i;
        m_strings.push_back(String(s));
        return m_strings.size() - 1;
    }
    int size() const
    {
        return m_strings.size();
    }
    const String& get(int i) const
    {
        return m_strings[i];
    }

  private:
    Array<String> m_strings;
};

} // namespace core
