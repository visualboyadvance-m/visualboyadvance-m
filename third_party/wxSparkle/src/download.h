/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _download_h_
#define _download_h_

#include <string>
#include <cstddef>

namespace winsparkle
{

class Thread;

/**
    Abstraction for storing downloaded data.
 */
struct IDownloadSink
{
    virtual ~IDownloadSink() {}

    /**
        Inform the sink of total data size.

        Note that this is not guaranteed to be called.
     */
    virtual void SetLength(size_t len) = 0;

    /**
       Inform the sink of detected filename.

       The filename is guaranteed to be a plain file name, without any
       directory components.
     */
    virtual void SetFilename(const std::wstring& filename) = 0;

    /// Add chunk of downloaded data
    virtual void Add(const void *data, size_t len) = 0;
};

/**
    IDownloadSink imlementation for storing data in a string.
 */
struct StringDownloadSink : public IDownloadSink
{
    virtual void SetLength(size_t) {}

    virtual void SetFilename(const std::wstring&) {}

    virtual void Add(const void *data, size_t len)
    {
        this->data.append(reinterpret_cast<const char*>(data), len);
    }

    /// Downloaded data, as a string.
    std::string data;
};


/// Flags for DownloadFile().
enum DownloadFlag
{
    /// Instruct proxies to pass the request upstream
    Download_BypassProxies = 1
};

/**
    Downloads a HTTP resource using libcurl.

    TLS certificates are verified against the CA bundle embedded into the
    library at build time (cert.pem).

    Throws on error.

    @param url       URL of the resource to download.
    @param sink      Where to put downloaded data.
    @param onThread  Thread the request runs on.
    @param headers   Extra HTTP headers, each terminated with "\r\n".
    @param flags     Or-combination of DownloadFlag values.
 */
void DownloadFile(const std::string& url, IDownloadSink *sink, Thread *onThread, const std::string &headers = "", int flags = 0);


/// Helpers exposed for unit testing.
namespace detail
{

/// Extracts the file name from a Content-Disposition header value (UTF-8).
/// Returns an empty string if there is no file name.
std::string ParseContentDispositionFilename(const std::string& value);

/// Extracts the (unescaped) file name from the path component of a URL (UTF-8).
std::string GetURLFileName(const std::string& url);

/// Makes the file name safe to use as a plain file name in a directory.
std::wstring SanitizeFileName(const std::wstring& name);

/// Returns the embedded CA bundle (PEM) and its size.
const unsigned char *GetEmbeddedCABundle(size_t *size);

} // namespace detail

} // namespace winsparkle

#endif // _download_h_
