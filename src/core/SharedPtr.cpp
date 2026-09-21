// Reconstructed from Grimrock.bin.x86 SharedPtr.cpp (0x080c9710).
#include "core/SharedPtr.h"

namespace core
{

HashMap<void*, int> SharedPtrBase::sm_refcount;

// Inlined in every SharedPtr<T>(T*) / reset(): look the pointer up in sm_refcount,
// insert with count 0 when missing, and return the address of the count.
int* SharedPtrBase::acquire(void* object)
{
    int* refCount = sm_refcount.findValue(object);
    if (!refCount)
        refCount = &sm_refcount.insert(object, 0).value();
    return refCount;
}

// Inlined in every ~SharedPtr<T>: drop the map entry once the count reaches zero.
void SharedPtrBase::release(void* object)
{
    sm_refcount.remove(object);
}

} // namespace core
