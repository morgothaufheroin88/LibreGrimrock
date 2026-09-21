// Reconstructed from Grimrock.bin.x86 (core::HashMap<K,V> template, instantiated in
// SharedPtr.cpp, Node.cpp, ImmediateMode.cpp, sys.cpp, Steam.cpp ...).
// 256 chained buckets, m_count after the bucket table. Node = {K key; V value; Node* next}.
// Bucket = fold8(FNV-1a(key bytes)); for String keys the hash runs over the characters;
// for pair<A,B> the folded hashes of both members are xor'ed.
#pragma once
#include "core/String.h"
#include <cstddef>
#include <new>
#include <utility>

namespace core
{

inline unsigned int fnv1aBytes(const void* data, size_t length)
{
    const unsigned char* bytes = (const unsigned char*)data;
    unsigned int hash = Fnv1a32Basis;
    for (size_t i = 0; i < length; ++i)
        hash = (hash ^ bytes[i]) * Fnv1a32Prime;
    return hash;
}
// Folds a 32-bit hash into a bucket index (NumBuckets = 256).
inline unsigned int foldHash8(unsigned int hash)
{
    return ((hash >> 8) ^ hash) & 0xff;
}

template <class K> struct HashMapHasher
{
    static unsigned int bucket(const K& key)
    {
        return foldHash8(fnv1aBytes(&key, sizeof(K)));
    }
    static bool equal(const K& a, const K& b)
    {
        return a == b;
    }
};
template <> struct HashMapHasher<String>
{
    static unsigned int bucket(const String& key)
    {
        return foldHash8(fnv1aBytes(key.c_str(), (size_t)key.size()));
    }
    static bool equal(const String& a, const String& b)
    {
        return a == b;
    }
};
template <class A, class B> struct HashMapHasher<std::pair<A, B>>
{
    static unsigned int bucket(const std::pair<A, B>& key)
    {
        return HashMapHasher<A>::bucket(key.first) ^ HashMapHasher<B>::bucket(key.second);
    }
    static bool equal(const std::pair<A, B>& a, const std::pair<A, B>& b)
    {
        return a.first == b.first && a.second == b.second;
    }
};

template <class K, class V> class HashMap
{
  public:
    static constexpr int NumBuckets = 256;
    struct Node
    {
        K key;
        V value;
        Node* next;
        Node(const K& k, const V& v, Node* n) : key(k), value(v), next(n) {}
    };

    // Original iterator layout: {HashMap* map; unsigned bucket; Node* node}.
    class iterator
    {
      public:
        iterator() : m_pMap(0), m_bucket(0), m_pNode(0) {}
        iterator(HashMap* map, unsigned int bucket, Node* node)
            : m_pMap(map), m_bucket(bucket), m_pNode(node)
        {
        }
        Node& operator*() const
        {
            return *m_pNode;
        }
        Node* operator->() const
        {
            return m_pNode;
        }
        Node* node() const
        {
            return m_pNode;
        }
        const K& key() const
        {
            return m_pNode->key;
        }
        V& value() const
        {
            return m_pNode->value;
        }
        bool operator==(const iterator& o) const
        {
            return m_pNode == o.m_pNode;
        }
        bool operator!=(const iterator& o) const
        {
            return m_pNode != o.m_pNode;
        }
        iterator& operator++()
        {
            if (m_pNode)
                m_pNode = m_pNode->next;
            while (!m_pNode && m_pMap && ++m_bucket < NumBuckets)
                m_pNode = m_pMap->m_buckets[m_bucket];
            if (m_bucket >= NumBuckets)
                m_pNode = 0;
            return *this;
        }

      private:
        HashMap* m_pMap;
        unsigned int m_bucket;
        Node* m_pNode;
    };

    HashMap() : m_count(0)
    {
        for (int i = 0; i < NumBuckets; ++i)
            m_buckets[i] = 0;
    }
    HashMap(const HashMap& other) : m_count(0)
    {
        for (int i = 0; i < NumBuckets; ++i)
            m_buckets[i] = 0;
        for (int i = 0; i < NumBuckets; ++i)
            for (Node* n = other.m_buckets[i]; n; n = n->next)
                insert(n->key, n->value);
    }
    HashMap& operator=(const HashMap& other)
    {
        if (this != &other)
        {
            clear();
            for (int i = 0; i < NumBuckets; ++i)
                for (Node* n = other.m_buckets[i]; n; n = n->next)
                    insert(n->key, n->value);
        }
        return *this;
    }
    // 0x080c9780 / 0x0812d2c0
    ~HashMap()
    {
        clear();
    }
    // 0x0812fb30
    void clear()
    {
        for (int i = 0; i < NumBuckets; ++i)
        {
            Node* n = m_buckets[i];
            while (n)
            {
                Node* next = n->next;
                delete n;
                n = next;
            }
            m_buckets[i] = 0;
        }
        m_count = 0;
    }

    // 0x080d2a40 / 0x0812f940: overwrite value if key exists, else prepend a node.
    iterator insert(const K& key, const V& value)
    {
        unsigned int b = HashMapHasher<K>::bucket(key);
        for (Node* n = m_buckets[b]; n; n = n->next)
        {
            if (HashMapHasher<K>::equal(n->key, key))
            {
                if (&n->value != &value)
                    n->value = value;
                return iterator(this, b, n);
            }
        }
        Node* n = new Node(key, value, m_buckets[b]);
        m_buckets[b] = n;
        ++m_count;
        return iterator(this, b, n);
    }
    iterator find(const K& key)
    {
        unsigned int b = HashMapHasher<K>::bucket(key);
        for (Node* n = m_buckets[b]; n; n = n->next)
            if (HashMapHasher<K>::equal(n->key, key))
                return iterator(this, b, n);
        return end();
    }
    const V* findValue(const K& key) const
    {
        unsigned int b = HashMapHasher<K>::bucket(key);
        for (Node* n = m_buckets[b]; n; n = n->next)
            if (HashMapHasher<K>::equal(n->key, key))
                return &n->value;
        return 0;
    }
    V* findValue(const K& key)
    {
        unsigned int b = HashMapHasher<K>::bucket(key);
        for (Node* n = m_buckets[b]; n; n = n->next)
            if (HashMapHasher<K>::equal(n->key, key))
                return &n->value;
        return 0;
    }
    bool contains(const K& key) const
    {
        return findValue(key) != 0;
    }
    V& operator[](const K& key)
    {
        V* v = findValue(key);
        if (v)
            return *v;
        return insert(key, V()).value();
    }
    // 0x080fe830
    bool remove(const K& key)
    {
        unsigned int b = HashMapHasher<K>::bucket(key);
        Node** link = &m_buckets[b];
        for (Node* n = m_buckets[b]; n; link = &n->next, n = n->next)
        {
            if (HashMapHasher<K>::equal(n->key, key))
            {
                *link = n->next;
                delete n;
                --m_count;
                return true;
            }
        }
        return false;
    }
    int size() const
    {
        return m_count;
    }
    bool empty() const
    {
        return m_count == 0;
    }
    iterator begin()
    {
        for (unsigned int i = 0; i < NumBuckets; ++i)
            if (m_buckets[i])
                return iterator(this, i, m_buckets[i]);
        return end();
    }
    iterator end()
    {
        return iterator(this, NumBuckets, 0);
    }

  private:
    friend class iterator;
    Node* m_buckets[NumBuckets];
    int m_count;
};

} // namespace core
