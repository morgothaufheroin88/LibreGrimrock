// Reconstructed from Grimrock.bin.x86 Exception.cpp.
#include "core/Exception.h"
#include <cstdarg>
#include <cstdio>

namespace core
{

char Exception::sm_strReason[2048];

// 0x080c6d00
Exception::Exception()
{
    sm_strReason[0] = 0;
}

// 0x080c6d80
Exception::Exception(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(sm_strReason, sizeof(sm_strReason), format, args);
    va_end(args);
}

// 0x080c6d50
void Exception::setReason(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(sm_strReason, sizeof(sm_strReason), format, args);
    va_end(args);
}

// 0x080c6d10
const char* Exception::getReason() const
{
    return sm_strReason;
}

// 0x080c6db0
void Exception::show()
{
    fprintf(stderr, "The following exception has occurred:\n%s\n", sm_strReason);
}

#define GRIMROCK_EXCEPTION_CTOR(Class)                                                             \
    Class::Class(const char* format, ...)                                                          \
    {                                                                                              \
        va_list args;                                                                              \
        va_start(args, format);                                                                    \
        char buffer[2048];                                                                         \
        vsnprintf(buffer, sizeof(buffer), format, args);                                           \
        va_end(args);                                                                              \
        setReason("%s", buffer);                                                                   \
    }
GRIMROCK_EXCEPTION_CTOR(FileNotFoundException)
GRIMROCK_EXCEPTION_CTOR(FileOpenFailedException)
GRIMROCK_EXCEPTION_CTOR(FileReadFailedException)
GRIMROCK_EXCEPTION_CTOR(FileWriteFailedException)
GRIMROCK_EXCEPTION_CTOR(FileSeekFailedException)
GRIMROCK_EXCEPTION_CTOR(BrokenArchiveException)
GRIMROCK_EXCEPTION_CTOR(InvalidFileFormatException)
GRIMROCK_EXCEPTION_CTOR(InvalidFileVersionException)
#undef GRIMROCK_EXCEPTION_CTOR

} // namespace core
