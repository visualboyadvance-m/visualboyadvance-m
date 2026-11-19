/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "appcontroller.h"
#include "updatedownloader.h"
#include "download.h"
#include "settings.h"
#include "ui.h"
#include "error.h"
#include "signatureverifier.h"
#include "platform.h"
#include "utils.h"

#if WXSPARKLE_CLASSIC_MAC
    #include <wx/filefn.h>
    #include <wx/filename.h>
    #include <wx/stdpaths.h>
#endif

#include <chrono>
#include <cstdio>
#include <random>
#include <system_error>

namespace fs = std::filesystem;

namespace winsparkle
{

/*--------------------------------------------------------------------------*
                                  helpers
 *--------------------------------------------------------------------------*/

namespace
{

#if WXSPARKLE_CLASSIC_MAC

// Classic Mac OS: HFS paths and wxWidgets file functions are used, as the
// POSIX file system API isn't available.
typedef wxString UpdatePath;

// (HFS names are limited to 31 characters, 17 + 2*6 are used)
const char TEMP_DIR_PREFIX[] = "wxSparkle Update ";
const size_t TEMP_DIR_RANDOM_BYTES = 6;

wxString GetTempDirectory()
{
    wxString tmp = wxStandardPaths::Get().GetTempDir();
    if ( tmp.empty() )
        throw std::runtime_error("Cannot create temporary directory: cannot determine temp dir");
    // (a volume's root, "Volume:", keeps its separator)
    if ( tmp.Last() == ':' && tmp.find(':') != tmp.length() - 1 )
        tmp.RemoveLast();
    return tmp;
}

// There's no OS source of randomness (std::random_device would throw), but
// the names don't need to be unpredictable on this single user OS, only
// unique.
std::string RandomHexString(size_t bytes)
{
    static unsigned s_counter = 0;
    std::seed_seq seed{
        unsigned(std::chrono::steady_clock::now().time_since_epoch().count()),
        unsigned(std::chrono::system_clock::now().time_since_epoch().count()),
        ++s_counter };
    std::mt19937 gen(seed);

    static const char hex[] = "0123456789abcdef";
    std::string s;
    for ( size_t i = 0; i < bytes; i++ )
    {
        const unsigned v = gen() & 0xFF;
        s += hex[v >> 4];
        s += hex[v & 0xF];
    }
    return s;
}

wxString CreateUniqueTempDirectory()
{
    const wxString tmpdir = GetTempDirectory();

    for ( int attempt = 0; attempt < 100; attempt++ )
    {
        const wxString name = wxString(TEMP_DIR_PREFIX) + RandomHexString(TEMP_DIR_RANDOM_BYTES);
        const wxString dir = wxFileName(tmpdir, name).GetFullPath();

        if ( wxFileName::DirExists(dir) || wxFileName::FileExists(dir) )
            continue; // already exists, try another name

        if ( !wxFileName::Mkdir(dir) )
            throw std::runtime_error("Cannot create temporary directory");
        return dir;
    }

    throw std::runtime_error("Cannot create temporary directory");
}

// Checks that the directory actually is a valid update temp dir, to prevent
// malicious users from forcing us into deleting arbitrary directories.
bool IsValidUpdateTempDir(const wxString& dir)
{
    // "::" is HFS's equivalent of "/../"
    if ( dir.empty() || dir.Contains("::") || dir[0] == ':' )
        return false;

    const wxFileName fn(dir);
    // (HFS names are case-insensitive)
    if ( !fn.GetPath().IsSameAs(GetTempDirectory(), false) )
        return false;

    const wxString name = fn.GetFullName();
    const wxString prefix(TEMP_DIR_PREFIX);
    return name.length() > prefix.length() && name.StartsWith(prefix);
}

#else // !WXSPARKLE_CLASSIC_MAC

typedef fs::path UpdatePath;

const size_t TEMP_DIR_RANDOM_BYTES = 16;

// Name prefix of the temporary directories. On Windows, the same prefix as in
// WinSparkle is used, so that leftovers from older versions are cleaned up.
#ifdef _WIN32
    const fs::path::value_type TEMP_DIR_PREFIX[] = L"Update-";
#else
    const fs::path::value_type TEMP_DIR_PREFIX[] = "wxSparkle-Update-";
#endif

fs::path NormalizedDir(const fs::path& p)
{
    fs::path n = p.lexically_normal();
    if ( !n.has_filename() && n.has_parent_path() ) // trailing separator
        n = n.parent_path();
    return n;
}

fs::path GetTempDirectory()
{
    std::error_code ec;
    const fs::path tmp = fs::temp_directory_path(ec);
    if ( ec || tmp.empty() )
        throw std::runtime_error("Cannot create temporary directory: cannot determine temp dir");
    return NormalizedDir(tmp);
}

std::string RandomHexString(size_t bytes)
{
    static const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::string s;
    for ( size_t i = 0; i < bytes; i++ )
    {
        const unsigned v = rd() & 0xFF;
        s += hex[v >> 4];
        s += hex[v & 0xF];
    }
    return s;
}

fs::path CreateUniqueTempDirectory()
{
    // We need to put downloaded updates into a directory of their own, because
    // if we put it in $TMP, some DLLs could be there and interfere with the
    // installer.
    //
    // This code creates a new randomized directory name and tries to create it;
    // this process is repeated if the directory already exists.
    const fs::path tmpdir = GetTempDirectory();

    for ( int attempt = 0; attempt < 100; attempt++ )
    {
        fs::path dir = tmpdir;
        fs::path::string_type name(TEMP_DIR_PREFIX);
        const std::string rnd = RandomHexString(TEMP_DIR_RANDOM_BYTES);
        name += fs::path(rnd).native();
        dir /= name;

        std::error_code ec;
        if ( fs::create_directory(dir, ec) )
        {
#ifndef _WIN32
            // only the current user should be able to access the files
            fs::permissions(dir, fs::perms::owner_all, fs::perm_options::replace, ec);
#endif
            return dir;
        }
        else if ( ec )
        {
            throw std::runtime_error("Cannot create temporary directory: " + ec.message());
        }
        // else: already exists, try another name
    }

    throw std::runtime_error("Cannot create temporary directory");
}

// Checks that the directory actually is a valid update temp dir, to prevent
// malicious users from forcing us into deleting arbitrary directories.
bool IsValidUpdateTempDir(const fs::path& dir)
{
    const fs::path n = NormalizedDir(dir);

    for ( const auto& component : n )
    {
        if ( component == ".." )
            return false;
    }

    if ( NormalizedDir(n.parent_path()) != GetTempDirectory() )
        return false;

    const fs::path::string_type name = n.filename().native();
    const fs::path::string_type prefix(TEMP_DIR_PREFIX);
    return name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0;
}

#endif // WXSPARKLE_CLASSIC_MAC/!WXSPARKLE_CLASSIC_MAC


struct UpdateDownloadSink : public IDownloadSink
{
    UpdateDownloadSink(Thread& thread, const UpdatePath& dir)
        : m_thread(thread),
          m_downloaded(0), m_total(0),
          m_file(NULL),
          m_dir(dir),
          m_lastUpdate(std::chrono::steady_clock::now() - std::chrono::seconds(1))
    {}

    ~UpdateDownloadSink()
    {
        try { Close(); } catch ( ... ) {} // destructors must not throw
    }

    void Close()
    {
        if ( m_file )
        {
            const bool ok = (fclose(m_file) == 0);
            m_file = NULL;
            if ( !ok )
                throw std::runtime_error("Cannot save update file");
        }
    }

#if WXSPARKLE_CLASSIC_MAC
    std::wstring GetFilePath(void) { return m_path.ToStdWstring(); }
#else
    std::wstring GetFilePath(void) { return PathToWide(m_path); }
#endif

    virtual void SetLength(size_t l) { m_total = l; }

    virtual void SetFilename(const std::wstring& filename)
    {
        if ( m_file )
            throw std::runtime_error("Update file already set");

#if WXSPARKLE_CLASSIC_MAC
        // DownloadFile() already sanitized the name, but be defensive; HFS
        // names are also limited to 31 characters (keep the extension):
        wxFileName fn(m_dir, wxString(detail::SanitizeFileName(filename)));
        wxString name = fn.GetName();
        const wxString ext = fn.GetExt();
        const size_t maxName = 31 - (ext.empty() ? 0 : ext.length() + 1);
        if ( ext.length() > 10 || maxName < 1 )
        {
            // not really an extension
            name = fn.GetFullName();
            if ( name.length() > 31 )
                name.Truncate(31);
            fn.SetFullName(name);
        }
        else if ( name.length() > maxName )
        {
            name.Truncate(maxName);
            fn.SetName(name);
        }
        m_path = fn.GetFullPath();

        if ( wxFileName::Exists(m_path) )
            throw std::runtime_error("Cannot save update file");
        // (the C library understands HFS paths in the file system encoding)
        const wxCharBuffer fsPath = m_path.fn_str();
        m_file = fsPath.data() ? fopen(fsPath.data(), "wb") : NULL;
#else
        // DownloadFile() already sanitized the name, but be defensive:
        m_path = m_dir / PathFromWide(detail::SanitizeFileName(filename));
#ifdef _WIN32
#if __STDC_WANT_SECURE_LIB__
        _wfopen_s(&m_file, m_path.c_str(), L"wb");
#else
        m_file = _wfopen(m_path.c_str(), L"wb");
#endif
#else
        m_file = fopen(m_path.c_str(), "wbx"); // fail if it exists
#endif
#endif // WXSPARKLE_CLASSIC_MAC/!WXSPARKLE_CLASSIC_MAC
        if ( !m_file )
            throw std::runtime_error("Cannot save update file");
    }

    virtual void Add(const void *data, size_t len)
    {
        if ( !m_file )
            throw std::runtime_error("Filename is not net");

        m_thread.CheckShouldTerminate();

        if ( fwrite(data, len, 1, m_file) != 1 )
            throw std::runtime_error("Cannot save update file");
        m_downloaded += len;

        // only update at most 10 times/sec so that we don't flood the UI:
        const auto now = std::chrono::steady_clock::now();
        if ( m_downloaded == m_total ||
             now - m_lastUpdate >= std::chrono::milliseconds(100) )
        {
            // compressed transfers may report misleading total size
            UI::NotifyDownloadProgress(m_downloaded, m_downloaded > m_total ? 0 : m_total);
            m_lastUpdate = now;
        }
    }

    Thread& m_thread;
    size_t m_downloaded, m_total;
    FILE *m_file;
    UpdatePath m_dir;
    UpdatePath m_path;
    std::chrono::steady_clock::time_point m_lastUpdate;
};

} // anonymous namespace


/*--------------------------------------------------------------------------*
                            updater initialization
 *--------------------------------------------------------------------------*/

UpdateDownloader::UpdateDownloader(const Appcast& appcast)
    : Thread("WinSparkle updater"),
      m_appcast(appcast)
{
}


/*--------------------------------------------------------------------------*
                              downloading
 *--------------------------------------------------------------------------*/

void UpdateDownloader::Run()
{
    // no initialization to do, so signal readiness immediately
    SignalReady();

    try
    {
      const UpdatePath tmpdir = CreateUniqueTempDirectory();
#if WXSPARKLE_CLASSIC_MAC
      Settings::WriteConfigValue("UpdateTempDir", tmpdir.ToStdWstring());
#else
      Settings::WriteConfigValue("UpdateTempDir", PathToWide(tmpdir));
#endif

      UpdateDownloadSink sink(*this, tmpdir);
      DownloadFile(m_appcast.enclosure.DownloadURL, &sink, this, Settings::GetHttpHeadersString());
      sink.Close();

      if (Settings::HasEdDSAPubKey())
      {
          SignatureVerifier::VerifyEdDSASignatureValid(sink.GetFilePath(), m_appcast.enclosure.EdDsaSignature);
      }
      else if (Settings::HasDSAPubKeyPem())
      {
          LogWarning("Using deprecated DSA signature. Please update your app to use EdDSA.");
          SignatureVerifier::VerifyDSASHA1SignatureValid(sink.GetFilePath(), m_appcast.enclosure.DsaSignature);
      }
      else
      {
          // backward compatibility - accept as is, but complain about it
          LogError("Using unsigned updates!");
      }

      UI::NotifyUpdateDownloaded(sink.GetFilePath(), m_appcast);
    }
    catch (TerminateThreadException&)
    {
        // cancelled by the user, this is not an error
        throw;
    }
    catch (BadSignatureException&)
    {
        CleanLeftovers();  // remove potentially corrupted file
        UI::NotifyUpdateError(Err_BadSignature);
        throw;
    }
    catch ( ... )
    {
        UI::NotifyUpdateError();
        throw;
    }
}


/*--------------------------------------------------------------------------*
                               cleanup
 *--------------------------------------------------------------------------*/

void UpdateDownloader::CleanLeftovers()
{
    // Note: this is called at startup. Do not use wxWidgets from this code!

    std::wstring tmpdirStr;
    if ( !Settings::ReadConfigValue("UpdateTempDir", tmpdirStr) )
        return;

#if WXSPARKLE_CLASSIC_MAC
    const wxString tmpdir(tmpdirStr);
#else
    const fs::path tmpdir = PathFromWide(tmpdirStr);
#endif

    // Check that the directory actually is a valid update temp dir, to prevent
    // malicious users from forcing us into deleting arbitrary directories:
    try
    {
        if ( !IsValidUpdateTempDir(tmpdir) )
        {
            Settings::DeleteConfigValue("UpdateTempDir");
            return;
        }
    }
    catch (std::exception&) // cannot determine temp directory
    {
        return;
    }

#if WXSPARKLE_CLASSIC_MAC
    // (aliases inside it are files, removing them doesn't affect the target)
    const bool ok = !wxFileName::DirExists(tmpdir) ||
                    wxFileName::Rmdir(tmpdir, wxPATH_RMDIR_RECURSIVE);
    if ( ok )
#else
    std::error_code ec;
    if ( fs::is_symlink(tmpdir, ec) )
        fs::remove(tmpdir, ec); // remove the link only, never its target
    else
        fs::remove_all(tmpdir, ec);

    if ( !ec )
#endif
    {
        Settings::DeleteConfigValue("UpdateTempDir");
    }
    // else: try another time, this is just a "soft" error
}

} // namespace winsparkle
