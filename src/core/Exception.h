// Reconstructed from Grimrock.bin.x86 Exception.cpp (0x080c6d00-0x080c6db0).
// The exception object itself is empty (sizeof == 1); the reason text lives in a
// static 2048-byte buffer shared by all exceptions.
#pragma once

namespace core
{

class Exception
{
  public:
    Exception();
    Exception(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void setReason(const char* format, ...) __attribute__((format(printf, 2, 3)));
    const char* getReason() const;
    static void show();

  private:
    static char sm_strReason[2048];
};

// Exception classes present in the binary's typeinfo list.
class FileNotFoundException : public Exception
{
  public:
    FileNotFoundException() {}
    FileNotFoundException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};
class FileOpenFailedException : public Exception
{
  public:
    FileOpenFailedException() {}
    FileOpenFailedException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};
class FileReadFailedException : public Exception
{
  public:
    FileReadFailedException() {}
    FileReadFailedException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};
class FileWriteFailedException : public Exception
{
  public:
    FileWriteFailedException() {}
    FileWriteFailedException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};
class FileSeekFailedException : public Exception
{
  public:
    FileSeekFailedException() {}
    FileSeekFailedException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};
class BrokenArchiveException : public Exception
{
  public:
    BrokenArchiveException() {}
    BrokenArchiveException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};
class InvalidFileFormatException : public Exception
{
  public:
    InvalidFileFormatException() {}
    InvalidFileFormatException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};
class InvalidFileVersionException : public Exception
{
  public:
    InvalidFileVersionException() {}
    InvalidFileVersionException(const char* format, ...) __attribute__((format(printf, 2, 3)));
};

} // namespace core
