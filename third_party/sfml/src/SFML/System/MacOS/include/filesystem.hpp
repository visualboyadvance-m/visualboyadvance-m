////////////////////////////////////////////////////////////
// The part of ghc::filesystem SFML uses, for classic Mac OS (Retro68).
//
// ghc::filesystem has no classic Mac OS backend (no dirent, no statvfs, and
// paths there are HFS paths: "Volume:Folder:File"). SFML only needs a path
// value it can pass to fopen()/fstream (FileInputStream, Ftp downloads and
// uploads), join, take the file name of and remove, so this stands in for
// it. A path is the text it was made from; joining uses ':' when the left
// side is an HFS path and '/' otherwise, which keeps Ftp's remote (Unix
// style) paths and its local (HFS) ones both right.
////////////////////////////////////////////////////////////

#pragma once

#include <cerrno>
#include <cstdio>
#include <string>
#include <system_error>
#include <utility>

namespace ghc::filesystem
{
class path
{
public:
    using value_type  = char;
    using string_type = std::string;

    path() = default;
    path(string_type text) : m_text(std::move(text))
    {
    }
    path(const value_type* text) : m_text(text ? text : "")
    {
    }
    template <class InputIt>
    path(InputIt first, InputIt last) : m_text(first, last)
    {
    }

    path& operator/=(const path& other)
    {
        if (other.m_text.empty())
            return *this;
        if (m_text.empty() || isAbsolute(other.m_text))
        {
            m_text = other.m_text;
            return *this;
        }
        const char sep = separator();
        if (m_text.back() != sep)
            m_text += sep;
        // A relative HFS path starts with ':'; don't double it.
        m_text += (sep == ':' && other.m_text.front() == ':') ? other.m_text.substr(1) : other.m_text;
        return *this;
    }

    path& operator+=(const path& other)
    {
        m_text += other.m_text;
        return *this;
    }

    [[nodiscard]] path filename() const
    {
        const std::string::size_type pos = m_text.find_last_of(":/");
        return pos == std::string::npos ? *this : path(m_text.substr(pos + 1));
    }

    [[nodiscard]] path parent_path() const
    {
        const std::string::size_type pos = m_text.find_last_of(":/");
        return pos == std::string::npos ? path() : path(m_text.substr(0, pos));
    }

    [[nodiscard]] path extension() const
    {
        const std::string name = filename().m_text;
        const std::string::size_type pos = name.rfind('.');
        return (pos == std::string::npos || pos == 0) ? path() : path(name.substr(pos));
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_text.empty();
    }

    [[nodiscard]] bool is_absolute() const
    {
        return isAbsolute(m_text);
    }

    [[nodiscard]] const string_type& native() const noexcept
    {
        return m_text;
    }
    [[nodiscard]] const value_type* c_str() const noexcept
    {
        return m_text.c_str();
    }
    [[nodiscard]] std::string string() const
    {
        return m_text;
    }
    [[nodiscard]] std::string u8string() const
    {
        return m_text;
    }
    [[nodiscard]] std::string generic_string() const
    {
        return m_text;
    }
    operator string_type() const
    {
        return m_text;
    }

    void clear() noexcept
    {
        m_text.clear();
    }

    friend bool operator==(const path& a, const path& b)
    {
        return a.m_text == b.m_text;
    }
    friend bool operator!=(const path& a, const path& b)
    {
        return a.m_text != b.m_text;
    }
    friend bool operator<(const path& a, const path& b)
    {
        return a.m_text < b.m_text;
    }
    friend path operator/(path a, const path& b)
    {
        a /= b;
        return a;
    }

    template <class Stream>
    friend Stream& operator<<(Stream& os, const path& p)
    {
        os << p.m_text;
        return os;
    }

private:
    // "Volume:..." is a full HFS path; ":..." a relative one; "/..." Unix.
    static bool isAbsolute(const std::string& text)
    {
        if (text.empty())
            return false;
        if (text.front() == '/')
            return true;
        const std::string::size_type colon = text.find(':');
        return colon != std::string::npos && colon != 0;
    }

    char separator() const
    {
        return m_text.find(':') != std::string::npos ? ':' : '/';
    }

    string_type m_text;
};

// There is no working directory to resolve against in a way callers could
// use; a path is reported as given.
inline path absolute(const path& p)
{
    return p;
}

inline path absolute(const path& p, std::error_code& ec)
{
    ec.clear();
    return p;
}

inline bool remove(const path& p)
{
    return std::remove(p.c_str()) == 0;
}

inline bool remove(const path& p, std::error_code& ec) noexcept
{
    if (std::remove(p.c_str()) == 0)
    {
        ec.clear();
        return true;
    }
    ec = std::error_code(errno, std::generic_category());
    return false;
}

} // namespace ghc::filesystem
