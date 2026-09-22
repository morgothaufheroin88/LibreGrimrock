// Reconstructed from Grimrock.bin.x86 SharedPtr.cpp (0x080c9710) and the many
// SharedPtr<T>::reset / ~SharedPtr instantiations (e.g. 0x0810ca30, 0x080e77c0).
// Intrusive-less shared pointer: reference counts live in a process-wide
// HashMap<void*, int> keyed by the object address (SharedPtrBase::sm_refcount).
// Layout {T* m_pObject; int* m_pRefCount}. Deletion goes through the virtual dtor.
#pragma once
#include "core/HashMap.h"

namespace core
{

class SharedPtrBase
{
  public:
    // sm_refcount of the original, kept alive through exit (see SharedPtr.cpp)
    static HashMap<void*, int>& refcounts();
    static int* acquire(void* object);
    static void release(void* object);
    static int objectCount()
    {
        return refcounts().size();
    }
};

template <class T> class SharedPtr
{
  public:
    SharedPtr() : m_pObject(0), m_pRefCount(0) {}
    explicit SharedPtr(T* p) : m_pObject(p), m_pRefCount(0)
    {
        if (p)
        {
            m_pRefCount = SharedPtrBase::acquire(p);
            ++*m_pRefCount;
        }
    }
    SharedPtr(const SharedPtr& other) : m_pObject(other.m_pObject), m_pRefCount(other.m_pRefCount)
    {
        if (m_pRefCount)
            ++*m_pRefCount;
    }
    template <class U>
    SharedPtr(const SharedPtr<U>& other) : m_pObject(other.get()), m_pRefCount(other.refCountPtr())
    {
        if (m_pRefCount)
            ++*m_pRefCount;
    }
    ~SharedPtr()
    {
        dispose();
    }
    SharedPtr& operator=(const SharedPtr& other)
    {
        SharedPtr(other).swap(*this);
        return *this;
    }
    template <class U> SharedPtr& operator=(const SharedPtr<U>& other)
    {
        SharedPtr(other).swap(*this);
        return *this;
    }
    // 0x0810ca30: *this = SharedPtr(p)
    void reset(T* p = 0)
    {
        SharedPtr(p).swap(*this);
    }
    void swap(SharedPtr& other)
    {
        T* p = m_pObject;
        int* rc = m_pRefCount;
        m_pObject = other.m_pObject;
        m_pRefCount = other.m_pRefCount;
        other.m_pObject = p;
        other.m_pRefCount = rc;
    }
    T* get() const
    {
        return m_pObject;
    }
    int* refCountPtr() const
    {
        return m_pRefCount;
    }
    T& operator*() const
    {
        return *m_pObject;
    }
    T* operator->() const
    {
        return m_pObject;
    }
    operator bool() const
    {
        return m_pObject != 0;
    }
    bool operator!() const
    {
        return m_pObject == 0;
    }
    int useCount() const
    {
        return m_pRefCount ? *m_pRefCount : 0;
    }

  private:
    // 0x080e77c0 ~SharedPtr
    void dispose()
    {
        if (m_pObject && --*m_pRefCount == 0)
        {
            SharedPtrBase::release(m_pObject);
            delete m_pObject;
        }
        m_pObject = 0;
        m_pRefCount = 0;
    }
    T* m_pObject;
    int* m_pRefCount;
};

template <class T, class U> bool operator==(const SharedPtr<T>& a, const SharedPtr<U>& b)
{
    return a.get() == b.get();
}
template <class T, class U> bool operator!=(const SharedPtr<T>& a, const SharedPtr<U>& b)
{
    return a.get() != b.get();
}
template <class T> bool operator==(const SharedPtr<T>& a, const T* b)
{
    return a.get() == b;
}
template <class T> bool operator!=(const SharedPtr<T>& a, const T* b)
{
    return a.get() != b;
}

// core::ScopedPtr<T>: single owner, deletes in destructor.
template <class T> class ScopedPtr
{
  public:
    ScopedPtr() : m_pObject(0) {}
    explicit ScopedPtr(T* p) : m_pObject(p) {}
    ~ScopedPtr()
    {
        delete m_pObject;
    }
    void reset(T* p = 0)
    {
        if (p != m_pObject)
        {
            delete m_pObject;
            m_pObject = p;
        }
    }
    T* release()
    {
        T* p = m_pObject;
        m_pObject = 0;
        return p;
    }
    T* get() const
    {
        return m_pObject;
    }
    T& operator*() const
    {
        return *m_pObject;
    }
    T* operator->() const
    {
        return m_pObject;
    }
    operator bool() const
    {
        return m_pObject != 0;
    }

  private:
    ScopedPtr(const ScopedPtr&);
    ScopedPtr& operator=(const ScopedPtr&);
    T* m_pObject;
};

} // namespace core
