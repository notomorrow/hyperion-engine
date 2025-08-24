#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <util/UTF8.hpp>

namespace hyperion {
namespace vm {

class Exception
{
public:
    Exception(const char *str);
    Exception(const Exception &other);
    Exception &operator=(const Exception &other);
    Exception(Exception &&other) noexcept;
    Exception &operator=(Exception &&other) noexcept;
    ~Exception();

    const char *ToString() const { return m_str; }

    static Exception InvalidComparisonException(const char *leftTypeStr, const char *rightTypeStr);
    static Exception InvalidOperationException(const char *opName,
        const char *leftTypeStr, const char *rightTypeStr);
    static Exception InvalidOperationException(const char *opName, const char *typeStr);
    static Exception InvalidBitwiseArgument();
    static Exception InvalidArgsException(int expected, int received, bool variadic = false);
    static Exception InvalidArgsException(const char *expectedStr, int received);
    static Exception InvalidArgsException(const char *expectedStr);
    static Exception InvalidConstructorException();
    static Exception NullReferenceException();
    static Exception DivisionByZeroException();
    static Exception OutOfBoundsException();
    static Exception MemberNotFoundException(uint32 hashCode);
    static Exception FileOpenException(const char *fileName);
    static Exception UnopenedFileWriteException();
    static Exception UnopenedFileReadException();
    static Exception UnopenedFileCloseException();
    static Exception LibraryLoadException(const char *libName);
    static Exception LibraryFunctionLoadException(const char *funcName);
    static Exception DuplicateExportException();
    static Exception KeyNotFoundException(const char *key);

private:
    char *m_str;
};

} // namespace vm
} // namespace hyperion

#endif
