/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "download.h"

#include "error.h"
#include "settings.h"
#include "threads.h"
#include "utils.h"
#include "winsparkle-version.h"

// Generated at build time from cert.pem by cmake/bin2c.cmake:
#include "wxsparkle_cacert.h"

#include <curl/curl.h>

#if WXSPARKLE_CLASSIC_MAC
    #include <pthread.h>
    #include <sys/socket.h> // libmactcp's, for mactcp_set_idle_func()
    // (Retro68's libcurl always uses OpenSSL)
    #include <openssl/err.h>
    #include <openssl/pem.h>
    #include <openssl/ssl.h>
    #include <openssl/x509.h>
#endif

#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <string>

#if LIBCURL_VERSION_NUM < 0x074D00
    #error "libcurl >= 7.77.0 is required (for CURLOPT_CAINFO_BLOB)"
#endif

namespace winsparkle
{

/*--------------------------------------------------------------------------*
                                helpers
 *--------------------------------------------------------------------------*/

namespace
{

// OS / architecture identification for the User-Agent header
#if defined(_WIN32)
    #define UA_OS "Windows"
#elif defined(__APPLE__)
    #define UA_OS "macOS"
#elif WXSPARKLE_CLASSIC_MAC
    #define UA_OS "Mac OS"
#elif defined(__ANDROID__) || defined(ANDROID)
    #define UA_OS "Android"
#elif defined(__linux__)
    #define UA_OS "Linux"
#elif defined(__FreeBSD__)
    #define UA_OS "FreeBSD"
#else
    #define UA_OS "Unix"
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    #define UA_ARCH "arm64"
#elif defined(__x86_64__) || defined(_M_X64)
    #define UA_ARCH "x64"
#elif defined(__i386__) || defined(_M_IX86)
    #define UA_ARCH "x86"
#elif defined(__arm__) || defined(_M_ARM)
    #define UA_ARCH "arm"
#elif defined(__riscv) && (__riscv_xlen == 64)
    #define UA_ARCH "riscv64"
#elif defined(__riscv)
    #define UA_ARCH "riscv"
#elif defined(__powerpc64__)
    #define UA_ARCH "ppc64"
#elif defined(__powerpc__) || defined(__ppc__)
    #define UA_ARCH "ppc"
#elif defined(__m68k__)
    #define UA_ARCH "m68k"
#else
    #define UA_ARCH "unknown"
#endif

std::string MakeUserAgent()
{
    std::string userAgent =
        ToUtf8(Settings::GetAppName()) + "/" + ToUtf8(Settings::GetAppVersion()) +
        " WinSparkle/" WIN_SPARKLE_VERSION_STRING " (wxSparkle; " UA_OS "; " UA_ARCH;

#if defined(_WIN32) && !defined(_WIN64)
    // If we're running a 32bit process, check if we're on 64bit Windows OS:
    auto f_IsWow64Process = LOAD_DYNAMIC_FUNC(IsWow64Process, kernel32);
    if( f_IsWow64Process )
    {
        BOOL wow64 = FALSE;
        f_IsWow64Process(GetCurrentProcess(), &wow64);
        if ( wow64 )
            userAgent += "; WOW64";
    }
#endif

    userAgent += ")";
    return userAgent;
}


CriticalSection g_curlInitCS;
bool g_curlInitialized = false;

#if WXSPARKLE_CLASSIC_MAC

// Threads are cooperative on classic Mac OS: while a transfer waits for the
// network, other threads -- in particular the main one running the UI --
// only get to run if the waiting thread yields. libmactcp calls an idle
// function while waiting, which does this; it also cancels the wait if the
// thread doing the transfer is asked to terminate.

pthread_key_t g_transferThreadKey;      // Thread* doing a transfer
mactcp_idle_func g_prevMacTcpIdle = NULL;

int MacTcpIdle()
{
    // the default one calls SystemTask(); chain to it
    int rc = g_prevMacTcpIdle ? g_prevMacTcpIdle() : 0;

    sched_yield();

    Thread *thread = static_cast<Thread*>(pthread_getspecific(g_transferThreadKey));
    if ( thread && thread->IsTerminationRequested() )
        rc = 1; // cancel the wait

    return rc;
}

// Associates the transfer with the current (OS) thread for MacTcpIdle().
class TransferThreadSetter
{
public:
    explicit TransferThreadSetter(Thread *thread)
    {
        pthread_setspecific(g_transferThreadKey, thread);
    }

    ~TransferThreadSetter()
    {
        pthread_setspecific(g_transferThreadKey, NULL);
    }

    TransferThreadSetter(const TransferThreadSetter&) = delete;
    TransferThreadSetter& operator=(const TransferThreadSetter&) = delete;
};

// Parsing the embedded CA bundle (~150 certificates) is very slow on classic
// Macs: it takes many seconds even on fast ones. libcurl would do it for
// every transfer, after connecting but before starting the TLS handshake,
// so servers drop the idle connection in the meantime (and, threads being
// cooperative, nothing else could run while it's parsed). Instead, parse it
// once, before connecting and yielding to other threads after every
// certificate, and give the resulting store to libcurl's SSL_CTX.

CriticalSection g_caStoreCS;
X509_STORE *g_caStore = NULL;

X509_STORE *GetCAStore(Thread *thread)
{
    CriticalSectionLocker lock(g_caStoreCS);
    if ( g_caStore )
        return g_caStore;

    X509_STORE *store = X509_STORE_new();
    BIO *bio = BIO_new_mem_buf(wxsparkle_cacert_pem, int(wxsparkle_cacert_pem_size));
    if ( !store || !bio )
    {
        X509_STORE_free(store);
        BIO_free(bio);
        throw std::runtime_error("Out of memory");
    }

    int count = 0;
    bool cancelled = false;
    while ( X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL) )
    {
        if ( X509_STORE_add_cert(store, cert) )
            count++;
        X509_free(cert);

        sched_yield();
        if ( thread && thread->IsTerminationRequested() )
        {
            cancelled = true;
            break;
        }
    }
    ERR_clear_error(); // (end of data is reported as an error)
    BIO_free(bio);

    if ( cancelled || count == 0 )
    {
        X509_STORE_free(store);
        if ( cancelled )
            throw Thread::TerminateThreadException();
        throw std::runtime_error("Failed to load the CA certificates");
    }

    g_caStore = store; // kept for the lifetime of the process
    return store;
}

CURLcode SetCAStoreCallback(CURL *, void *sslctx, void *store)
{
    // (takes a reference to the store)
    SSL_CTX_set1_cert_store(static_cast<SSL_CTX*>(sslctx), static_cast<X509_STORE*>(store));
    return CURLE_OK;
}

#endif // WXSPARKLE_CLASSIC_MAC

void EnsureCurlInitialized()
{
    CriticalSectionLocker lock(g_curlInitCS);
    if ( g_curlInitialized )
        return;

#if WXSPARKLE_CLASSIC_MAC
    const int err = pthread_key_create(&g_transferThreadKey, NULL);
    if ( err )
        throw std::runtime_error(std::string("Failed to initialize networking: ") + strerror(err));
    g_prevMacTcpIdle = mactcp_set_idle_func(&MacTcpIdle);
#endif

    const CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
    if ( rc != CURLE_OK )
        throw std::runtime_error(std::string("Failed to initialize libcurl: ") + curl_easy_strerror(rc));
    g_curlInitialized = true;
}


bool StartsWithNoCase(const std::string& s, const char *prefix)
{
    const size_t len = strlen(prefix);
    if ( s.size() < len )
        return false;
    for ( size_t i = 0; i < len; ++i )
    {
        if ( tolower(static_cast<unsigned char>(s[i])) != tolower(static_cast<unsigned char>(prefix[i])) )
            return false;
    }
    return true;
}

std::string Trim(const std::string& s)
{
    const size_t start = s.find_first_not_of(" \t\r\n");
    if ( start == std::string::npos )
        return std::string();
    const size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return s;
}

std::string PercentDecode(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for ( size_t i = 0; i < s.size(); ++i )
    {
        if ( s[i] == '%' && i + 2 < s.size() &&
             isxdigit(static_cast<unsigned char>(s[i + 1])) &&
             isxdigit(static_cast<unsigned char>(s[i + 2])) )
        {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        }
        else
        {
            out += s[i];
        }
    }
    return out;
}


struct CurlEasyDeleter
{
    void operator()(CURL *c) const { curl_easy_cleanup(c); }
};

struct CurlSlistDeleter
{
    void operator()(curl_slist *l) const { curl_slist_free_all(l); }
};


// State shared by the libcurl callbacks of a single transfer.
struct TransferContext
{
    CURL               *curl = nullptr;
    IDownloadSink      *sink = nullptr;
    Thread             *thread = nullptr;
    std::string         url;

    // headers of the *last* response (redirects reset them)
    std::string         contentDisposition;
    std::string         contentEncoding;

    bool                headersProcessed = false;
    std::exception_ptr  error;
};


// Called once, before the first chunk of data (or after the transfer, if
// there was no body), to pass response metadata to the sink.
void ProcessResponseHeaders(TransferContext& ctx)
{
    ctx.headersProcessed = true;

    // Check returned status code - we need to detect 404 instead of
    // downloading the human-readable 404 page:
    long statusCode = 0;
    curl_easy_getinfo(ctx.curl, CURLINFO_RESPONSE_CODE, &statusCode);
    if ( statusCode >= 400 )
    {
        throw std::runtime_error("Update file not found on the server (HTTP " +
                                 std::to_string(statusCode) + ").");
    }

    // Get content length if possible. Note that if the content is compressed,
    // Content-Length is the compressed size and useless for progress reporting.
    curl_off_t contentLength = -1;
    if ( curl_easy_getinfo(ctx.curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength) == CURLE_OK &&
         contentLength >= 0 &&
         (ctx.contentEncoding.empty() || ToLower(ctx.contentEncoding) == "identity") )
    {
        ctx.sink->SetLength(static_cast<size_t>(contentLength));
    }

    // Get filename from Content-Disposition, if available, or from the
    // (final, after redirects) URL otherwise:
    std::string filename = detail::ParseContentDispositionFilename(ctx.contentDisposition);
    if ( filename.empty() )
    {
        char *effectiveUrl = nullptr;
        if ( curl_easy_getinfo(ctx.curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl) == CURLE_OK && effectiveUrl )
            filename = detail::GetURLFileName(effectiveUrl);
        if ( filename.empty() )
            filename = detail::GetURLFileName(ctx.url);
    }

    ctx.sink->SetFilename(detail::SanitizeFileName(FromUtf8(filename)));
}


extern "C" size_t CurlHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    TransferContext& ctx = *static_cast<TransferContext*>(userdata);
    const size_t len = size * nitems;

    try
    {
        const std::string line(buffer, len);

        if ( StartsWithNoCase(line, "HTTP/") )
        {
            // new response (e.g. after a redirect): forget previous headers
            ctx.contentDisposition.clear();
            ctx.contentEncoding.clear();
        }
        else if ( StartsWithNoCase(line, "Content-Disposition:") )
        {
            ctx.contentDisposition = Trim(line.substr(strlen("Content-Disposition:")));
        }
        else if ( StartsWithNoCase(line, "Content-Encoding:") )
        {
            ctx.contentEncoding = Trim(line.substr(strlen("Content-Encoding:")));
        }
    }
    catch ( ... )
    {
        ctx.error = std::current_exception();
        return 0; // abort the transfer
    }

    return len;
}


extern "C" size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    TransferContext& ctx = *static_cast<TransferContext*>(userdata);
    const size_t len = size * nmemb;

    try
    {
        if ( !ctx.headersProcessed )
            ProcessResponseHeaders(ctx);

        if ( len )
            ctx.sink->Add(ptr, len);
    }
    catch ( ... )
    {
        ctx.error = std::current_exception();
#ifdef CURL_WRITEFUNC_ERROR
        return CURL_WRITEFUNC_ERROR;
#else
        return len ? 0 : 1; // anything != len aborts the transfer
#endif
    }

    return len;
}


extern "C" int CurlProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
{
    TransferContext& ctx = *static_cast<TransferContext*>(clientp);

    // abort the transfer if the thread was asked to terminate
    if ( ctx.thread && ctx.thread->IsTerminationRequested() )
        return 1;

    return 0;
}


void SetOpt(CURLcode rc, const char *what)
{
    if ( rc != CURLE_OK )
    {
        throw std::runtime_error(std::string("Failed to set libcurl option ") + what +
                                 ": " + curl_easy_strerror(rc));
    }
}

#define SET_CURL_OPT(curl, opt, val) SetOpt(curl_easy_setopt(curl, opt, val), #opt)

} // anonymous namespace


/*--------------------------------------------------------------------------*
                             helpers for testing
 *--------------------------------------------------------------------------*/

namespace detail
{

std::string ParseContentDispositionFilename(const std::string& value)
{
    // Parameters are separated by ';'. Prefer RFC 5987/6266 "filename*"
    // (e.g. filename*=UTF-8''na%C3%AFve.exe) over plain "filename".
    std::string plain, extended;

    size_t pos = 0;
    while ( pos < value.size() )
    {
        // find the end of this parameter, respecting quoted strings
        size_t end = pos;
        bool inQuotes = false;
        for ( ; end < value.size(); ++end )
        {
            const char c = value[end];
            if ( c == '"' )
                inQuotes = !inQuotes;
            else if ( c == '\\' && inQuotes && end + 1 < value.size() )
                ++end;
            else if ( c == ';' && !inQuotes )
                break;
        }

        const std::string param = Trim(value.substr(pos, end - pos));
        pos = end + 1;

        const size_t eq = param.find('=');
        if ( eq == std::string::npos )
            continue;

        const std::string name = ToLower(Trim(param.substr(0, eq)));
        std::string val = Trim(param.substr(eq + 1));

        if ( name == "filename*" )
        {
            // charset'language'percent-encoded-value
            const size_t q1 = val.find('\'');
            const size_t q2 = (q1 == std::string::npos) ? std::string::npos : val.find('\'', q1 + 1);
            if ( q2 != std::string::npos )
            {
                const std::string charset = ToLower(val.substr(0, q1));
                const std::string decoded = PercentDecode(val.substr(q2 + 1));
                if ( charset == "utf-8" )
                {
                    extended = decoded;
                }
                else // ISO-8859-1 or unknown: convert Latin-1 to UTF-8
                {
                    std::wstring w;
                    for ( unsigned char c : decoded )
                        w += static_cast<wchar_t>(c);
                    extended = ToUtf8(w);
                }
            }
        }
        else if ( name == "filename" )
        {
            if ( val.size() >= 2 && (val.front() == '"' || val.front() == '\'') && val.back() == val.front() )
            {
                const char quote = val.front();
                std::string unq;
                for ( size_t i = 1; i + 1 < val.size(); ++i )
                {
                    if ( val[i] == '\\' && quote == '"' && i + 2 < val.size() )
                        ++i;
                    unq += val[i];
                }
                val = unq;
            }
            plain = val;
        }
    }

    return !extended.empty() ? extended : plain;
}


std::string GetURLFileName(const std::string& url)
{
    std::string path = url;

    // strip fragment and query
    const size_t hash = path.find('#');
    if ( hash != std::string::npos )
        path.erase(hash);
    const size_t query = path.find('?');
    if ( query != std::string::npos )
        path.erase(query);

    // skip scheme://authority
    const size_t scheme = path.find("://");
    if ( scheme != std::string::npos )
    {
        const size_t pathStart = path.find('/', scheme + 3);
        path = (pathStart == std::string::npos) ? std::string() : path.substr(pathStart);
    }

    const size_t lastSlash = path.find_last_of('/');
    const std::string fn = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
    return PercentDecode(fn);
}


std::wstring SanitizeFileName(const std::wstring& name)
{
    // Never trust the server: use only the last path component and remove
    // characters that are invalid in file names on any supported platform.
    std::wstring fn = name;
    const size_t sep = fn.find_last_of(L"/\\");
    if ( sep != std::wstring::npos )
        fn = fn.substr(sep + 1);

    std::wstring out;
    for ( wchar_t c : fn )
    {
        if ( c < 0x20 || c == 0x7F ||
             c == L':' || c == L'*' || c == L'?' || c == L'"' ||
             c == L'<' || c == L'>' || c == L'|' )
        {
            out += L'_';
        }
        else
        {
            out += c;
        }
    }

    // trailing dots and spaces are problematic on Windows
    while ( !out.empty() && (out.back() == L'.' || out.back() == L' ') )
        out.pop_back();
    // leading dots would make the file hidden (or be "." / "..")
    while ( !out.empty() && out.front() == L'.' )
        out.erase(0, 1);

    if ( out.empty() )
        out = L"update";

    return out;
}


const unsigned char *GetEmbeddedCABundle(size_t *size)
{
    if ( size )
        *size = wxsparkle_cacert_pem_size;
    return wxsparkle_cacert_pem;
}

} // namespace detail


/*--------------------------------------------------------------------------*
                                public functions
 *--------------------------------------------------------------------------*/

void DownloadFile(const std::string& url, IDownloadSink* sink, Thread* onThread, const std::string& headers, int flags)
{
    EnsureCurlInitialized();

#if WXSPARKLE_CLASSIC_MAC
    TransferThreadSetter transferThread(onThread);
#endif

    std::unique_ptr<CURL, CurlEasyDeleter> curl(curl_easy_init());
    if ( !curl )
        throw std::runtime_error("Failed to create libcurl handle");

    CURL *c = curl.get();

    TransferContext ctx;
    ctx.curl = c;
    ctx.sink = sink;
    ctx.thread = onThread;
    ctx.url = url;

    // Custom headers, each terminated with "\r\n":
    curl_slist *rawHeaders = nullptr;
    std::unique_ptr<curl_slist, CurlSlistDeleter> headerList;
    auto addHeader = [&](const std::string& h)
    {
        curl_slist *l = curl_slist_append(rawHeaders, h.c_str());
        if ( !l )
            throw std::runtime_error("Out of memory");
        rawHeaders = l;
        headerList.release();
        headerList.reset(rawHeaders);
    };

    size_t start = 0;
    while ( start < headers.size() )
    {
        size_t end = headers.find("\r\n", start);
        if ( end == std::string::npos )
            end = headers.size();
        const std::string line = Trim(headers.substr(start, end - start));
        if ( !line.empty() )
            addHeader(line);
        start = end + 2;
    }

    // For some requests (appcast feeds), don't even allow proxies to cache,
    // as we need the most up-to-date information.
    if ( flags & Download_BypassProxies )
    {
        addHeader("Pragma: no-cache");
        addHeader("Cache-Control: no-cache");
    }

    char errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = '\0';

    const std::string userAgent = MakeUserAgent();
    const bool isHttps = StartsWithNoCase(url, "https://");

    SET_CURL_OPT(c, CURLOPT_URL, url.c_str());
    SET_CURL_OPT(c, CURLOPT_ERRORBUFFER, errorBuffer);
    SET_CURL_OPT(c, CURLOPT_USERAGENT, userAgent.c_str());
    if ( rawHeaders )
        SET_CURL_OPT(c, CURLOPT_HTTPHEADER, rawHeaders);
    SET_CURL_OPT(c, CURLOPT_NOSIGNAL, 1L);

    // only HTTP(S) is allowed; never follow redirects from HTTPS to plain HTTP
#if LIBCURL_VERSION_NUM >= 0x075500
    SET_CURL_OPT(c, CURLOPT_PROTOCOLS_STR, "http,https");
    SET_CURL_OPT(c, CURLOPT_REDIR_PROTOCOLS_STR, isHttps ? "https" : "http,https");
#else
    SET_CURL_OPT(c, CURLOPT_PROTOCOLS, long(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    SET_CURL_OPT(c, CURLOPT_REDIR_PROTOCOLS, isHttps ? long(CURLPROTO_HTTPS) : long(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    SET_CURL_OPT(c, CURLOPT_FOLLOWLOCATION, 1L);
    SET_CURL_OPT(c, CURLOPT_MAXREDIRS, 10L);

    // transparently decompress any encoding supported by libcurl
    SET_CURL_OPT(c, CURLOPT_ACCEPT_ENCODING, "");
    // prefer HTTP/2 over TLS when available (ignored if unsupported)
    curl_easy_setopt(c, CURLOPT_HTTP_VERSION, long(CURL_HTTP_VERSION_2TLS));

    // timeouts: give up on stalled connections, but allow slow downloads
    SET_CURL_OPT(c, CURLOPT_CONNECTTIMEOUT, 30L);
    SET_CURL_OPT(c, CURLOPT_LOW_SPEED_LIMIT, 1L);
    SET_CURL_OPT(c, CURLOPT_LOW_SPEED_TIME, 60L);

    // TLS: verify against the CA certificates embedded from cert.pem
    SET_CURL_OPT(c, CURLOPT_SSL_VERIFYPEER, 1L);
    SET_CURL_OPT(c, CURLOPT_SSL_VERIFYHOST, 2L);
#if WXSPARKLE_CLASSIC_MAC
    // (see GetCAStore(); only needed for HTTPS, but redirects may lead there)
    SET_CURL_OPT(c, CURLOPT_SSL_CTX_FUNCTION, &SetCAStoreCallback);
    SET_CURL_OPT(c, CURLOPT_SSL_CTX_DATA, GetCAStore(onThread));
    // don't use any CA file configured into libcurl
    curl_easy_setopt(c, CURLOPT_CAINFO, static_cast<char*>(nullptr));
#else
    curl_blob caBlob;
    caBlob.data = const_cast<unsigned char*>(wxsparkle_cacert_pem);
    caBlob.len = wxsparkle_cacert_pem_size;
    caBlob.flags = CURL_BLOB_NOCOPY;
    SET_CURL_OPT(c, CURLOPT_CAINFO_BLOB, &caBlob);
#endif
    // don't use any CA directory configured into libcurl
    curl_easy_setopt(c, CURLOPT_CAPATH, static_cast<char*>(nullptr));
#if defined(WXSPARKLE_TRUST_NATIVE_CA) && defined(CURLSSLOPT_NATIVE_CA)
    // additionally trust the operating system's certificate store
    curl_easy_setopt(c, CURLOPT_SSL_OPTIONS, long(CURLSSLOPT_NATIVE_CA));
#endif

    // fail on HTTP errors >= 400 instead of downloading error pages
    SET_CURL_OPT(c, CURLOPT_FAILONERROR, 1L);

    // callbacks
    SET_CURL_OPT(c, CURLOPT_HEADERFUNCTION, &CurlHeaderCallback);
    SET_CURL_OPT(c, CURLOPT_HEADERDATA, &ctx);
    SET_CURL_OPT(c, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
    SET_CURL_OPT(c, CURLOPT_WRITEDATA, &ctx);
    SET_CURL_OPT(c, CURLOPT_XFERINFOFUNCTION, &CurlProgressCallback);
    SET_CURL_OPT(c, CURLOPT_XFERINFODATA, &ctx);
    SET_CURL_OPT(c, CURLOPT_NOPROGRESS, 0L);

    const CURLcode rc = curl_easy_perform(c);

    // errors thrown from our callbacks (including thread termination) take
    // precedence over libcurl's own error reporting
    if ( ctx.error )
        std::rethrow_exception(ctx.error);

    if ( onThread )
        onThread->CheckShouldTerminate();

    if ( rc == CURLE_HTTP_RETURNED_ERROR )
    {
        long statusCode = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &statusCode);
        throw std::runtime_error("Update file not found on the server (HTTP " +
                                 std::to_string(statusCode) + ").");
    }

    if ( rc != CURLE_OK )
    {
        std::string msg("Download of ");
        msg += url;
        msg += " failed: ";
        msg += errorBuffer[0] ? errorBuffer : curl_easy_strerror(rc);
        throw std::runtime_error(msg);
    }

    // empty response body: the write callback was never called
    if ( !ctx.headersProcessed )
        ProcessResponseHeaders(ctx);
}

} // namespace winsparkle
