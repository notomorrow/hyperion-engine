#include <script/compiler/ErrorList.hpp>
#include <util/Termcolor.hpp>
#include <core/utilities/StringUtil.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/FixedArray.hpp>

#include <unordered_set>
#include <map>
#include <fstream>

namespace hyperion::compiler {

ErrorList::ErrorList()
    : m_errorSuppressionDepth(0)
{
}

ErrorList::ErrorList(const ErrorList &other)
    : m_errors(other.m_errors),
      m_errorSuppressionDepth(other.m_errorSuppressionDepth)
{
}

ErrorList &ErrorList::operator=(const ErrorList &other)
{
    m_errors = other.m_errors;
    m_errorSuppressionDepth = other.m_errorSuppressionDepth;

    return *this;
}

ErrorList::ErrorList(ErrorList &&other) noexcept
    : m_errors(std::move(other.m_errors)),
      m_errorSuppressionDepth(other.m_errorSuppressionDepth)
{
    other.m_errorSuppressionDepth = 0;
}

ErrorList &ErrorList::operator=(ErrorList &&other) noexcept
{
    m_errors = std::move(other.m_errors);
    m_errorSuppressionDepth = other.m_errorSuppressionDepth;

    other.m_errorSuppressionDepth = 0;

    return *this;
}

bool ErrorList::HasFatalErrors() const
{
    return m_errors.FindIf([](const CompilerError &error)
    {
        return error.GetLevel() == LEVEL_ERROR;
    }) != m_errors.End();
}

std::ostream &ErrorList::WriteOutput(std::ostream &os) const
{
    FlatSet<String> errorFilenames;
    Array<std::string> currentFileLines;

    for (const CompilerError &error : m_errors) {
        const String &path = error.GetLocation().GetFileName();

        if (errorFilenames.Insert(path).second) {
            currentFileLines.Clear();
            // read lines into currentLines vector
            // this is used so that we can print out the line that an error occured on
            std::ifstream is(path.Data());
            if (is.isOpen()) {
                std::string line;
                while (std::getline(is, line)) {
                    currentFileLines.PushBack(line);
                }
            }

            Array<String> split = path.Split('\\', '/');

            String realFilename = split.Any()
                ? split.Back()
                : path;

            realFilename = StringUtil::StripExtension(realFilename);

            os << termcolor::reset << "In file \"" << realFilename << "\":\n";
        }

        const String &errorText = error.GetText();

        switch (error.GetLevel()) {
        case LEVEL_INFO:
            os << termcolor::white << termcolor::onBlue << termcolor::bold << "Info";
            break;
        case LEVEL_WARN:
            os << termcolor::white << termcolor::onYellow << termcolor::bold << "Warning";
            break;
        case LEVEL_ERROR:
            os << termcolor::white << termcolor::onRed << termcolor::bold << "Error";
            break;
        }

        os << termcolor::reset
           << " at line "    << (error.GetLocation().GetLine() + 1)
           << ", col " << (error.GetLocation().GetColumn() + 1);

        os << ": " << errorText << '\n';

        if (currentFileLines.Size() > error.GetLocation().GetLine()) {
            // render the line in question
            os << "\n\t" << currentFileLines[error.GetLocation().GetLine()];
            os << "\n\t";

            for (SizeType i = 0; i < error.GetLocation().GetColumn(); i++) {
                os << ' ';
            }

            os << termcolor::green << "^";
        } else {
            os << '\t' << "<line not found>";
        }

        os << termcolor::reset << '\n';
    }

    return os;
}

} // namespace hyperion::compiler
