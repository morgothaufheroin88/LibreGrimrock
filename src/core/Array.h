// Reconstructed from Grimrock.bin.x86 (core::Array<T> template, inlined everywhere).
// Layout {T* m_pData; int m_size; int m_capacity}. Storage is malloc'ed, elements are
// placement-constructed. Growth: capacity = capacity * 3 / 2 until >= requested.
#pragma once
#include <cstdlib>
#include <new>
#include <utility>

namespace core
{

template <class T> class Array
{
  public:
    typedef T* iterator;
    typedef const T* const_iterator;

    Array() : m_pData(0), m_size(0), m_capacity(0) {}
    explicit Array(int n) : m_pData(0), m_size(0), m_capacity(0)
    {
        resize(n);
    }
    Array(const Array& other) : m_pData(0), m_size(0), m_capacity(0)
    {
        reserve(other.m_size);
        for (int i = 0; i < other.m_size; ++i)
            new (&m_pData[i]) T(other.m_pData[i]);
        m_size = other.m_size;
    }
    ~Array()
    {
        for (int i = 0; i < m_size; ++i)
            m_pData[i].~T();
        free(m_pData);
    }
    Array& operator=(const Array& other)
    {
        if (this != &other)
        {
            clear();
            reserve(other.m_size);
            for (int i = 0; i < other.m_size; ++i)
                new (&m_pData[i]) T(other.m_pData[i]);
            m_size = other.m_size;
        }
        return *this;
    }

    // 0x080c6a20 core::Array<core::String>::reserve(int)
    void reserve(int n)
    {
        if (m_capacity < n)
        {
            int cap = m_capacity;
            do
            {
                cap = (cap * 3) / 2;
                if (cap < n)
                    cap = n;
            } while (cap < n);
            m_capacity = cap;
            T* p = (T*)malloc((size_t)cap * sizeof(T));
            for (int i = 0; i < m_size; ++i)
            {
                new (&p[i]) T(m_pData[i]);
                m_pData[i].~T();
            }
            free(m_pData);
            m_pData = p;
        }
    }
    void resize(int n)
    {
        if (n > m_size)
        {
            reserve(n);
            for (int i = m_size; i < n; ++i)
                new (&m_pData[i]) T();
        }
        else
        {
            for (int i = n; i < m_size; ++i)
                m_pData[i].~T();
        }
        m_size = n;
    }
    void resize(int n, const T& value)
    {
        if (n > m_size)
        {
            reserve(n);
            for (int i = m_size; i < n; ++i)
                new (&m_pData[i]) T(value);
        }
        else
        {
            for (int i = n; i < m_size; ++i)
                m_pData[i].~T();
        }
        m_size = n;
    }
    // 0x080c95d0 core::Array<core::String>::push_back
    void push_back(const T& value)
    {
        reserve(m_size + 1);
        T* p = &m_pData[m_size++];
        new (p) T(value);
    }
    T& push_back()
    {
        reserve(m_size + 1);
        T* p = &m_pData[m_size++];
        new (p) T();
        return *p;
    }
    void pop_back()
    {
        if (m_size > 0)
            m_pData[--m_size].~T();
    }
    void insert(int index, const T& value)
    {
        if (index < 0 || index > m_size)
            return;
        reserve(m_size + 1);
        new (&m_pData[m_size]) T();
        for (int i = m_size; i > index; --i)
            m_pData[i] = m_pData[i - 1];
        m_pData[index] = value;
        ++m_size;
    }
    // Erase by shifting down (matches the inlined unmount()/FileSystem dtor loops).
    void erase(int index)
    {
        if (index < 0 || index >= m_size)
            return;
        for (int i = index; i < m_size - 1; ++i)
            m_pData[i] = m_pData[i + 1];
        m_pData[m_size - 1].~T();
        --m_size;
    }
    void erase(iterator it)
    {
        erase((int)(it - m_pData));
    }
    // Unordered erase: overwrite with last element.
    void eraseFast(int index)
    {
        if (index < 0 || index >= m_size)
            return;
        if (index != m_size - 1)
            m_pData[index] = m_pData[m_size - 1];
        m_pData[m_size - 1].~T();
        --m_size;
    }
    bool remove(const T& value)
    {
        int i = find(value);
        if (i == -1)
            return false;
        erase(i);
        return true;
    }
    int find(const T& value) const
    {
        for (int i = 0; i < m_size; ++i)
            if (m_pData[i] == value)
                return i;
        return -1;
    }
    bool contains(const T& value) const
    {
        return find(value) != -1;
    }
    void clear()
    {
        for (int i = 0; i < m_size; ++i)
            m_pData[i].~T();
        m_size = 0;
    }
    void swap(Array& other)
    {
        std::swap(m_pData, other.m_pData);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    int size() const
    {
        return m_size;
    }
    int capacity() const
    {
        return m_capacity;
    }
    bool empty() const
    {
        return m_size == 0;
    }
    T& operator[](int i)
    {
        return m_pData[i];
    }
    const T& operator[](int i) const
    {
        return m_pData[i];
    }
    T& front()
    {
        return m_pData[0];
    }
    const T& front() const
    {
        return m_pData[0];
    }
    T& back()
    {
        return m_pData[m_size - 1];
    }
    const T& back() const
    {
        return m_pData[m_size - 1];
    }
    T* data()
    {
        return m_pData;
    }
    const T* data() const
    {
        return m_pData;
    }
    iterator begin()
    {
        return m_pData;
    }
    iterator end()
    {
        return m_pData + m_size;
    }
    const_iterator begin() const
    {
        return m_pData;
    }
    const_iterator end() const
    {
        return m_pData + m_size;
    }

  private:
    T* m_pData;
    int m_size;
    int m_capacity;
};

} // namespace core
