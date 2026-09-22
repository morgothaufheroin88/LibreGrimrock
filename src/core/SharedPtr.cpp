// Reconstructed from Grimrock.bin.x86 SharedPtr.cpp (0x080c9710).
#include "core/SharedPtr.h"

namespace core
{

// The map outlives every static SharedPtr (the asset processor and mesh registries
// release their objects during exit), so it is never destroyed.
HashMap<void*, int>& SharedPtrBase::refcounts()
{
    static HashMap<void*, int>* map = new HashMap<void*, int>;
    return *map;
}

// Inlined in every SharedPtr<T>(T*) / reset(): look the pointer up in the map, insert
// with count 0 when missing, and return the address of the count.
int* SharedPtrBase::acquire(void* object)
{
    HashMap<void*, int>& map = refcounts();
    int* refCount = map.findValue(object);
    if (!refCount)
        refCount = &map.insert(object, 0).value();
    return refCount;
}

// Inlined in every ~SharedPtr<T>: drop the map entry once the count reaches zero.
void SharedPtrBase::release(void* object)
{
    refcounts().remove(object);
}

} // namespace core
