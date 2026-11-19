/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

// Unit tests for wxSparkle internals. Set WXSPARKLE_TEST_NETWORK=1 to also
// run tests that download data from the internet.

#include "appcast.h"
#include "download.h"
#include "settings.h"
#include "signatureverifier.h"
#include "threads.h"
#include "updatechecker.h"
#include "utils.h"

#include <winsparkle.h>
#include <ed25519.h>

#include <wx/init.h>

#if WXSPARKLE_HAS_OPENSSL
    #include <openssl/evp.h>
    #include <openssl/pem.h>
#endif

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

using namespace winsparkle;
namespace fs = std::filesystem;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        g_checks++;                                                         \
        if ( !(cond) ) {                                                    \
            g_failures++;                                                   \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: "  \
                      << #cond << std::endl;                                \
        }                                                                   \
    } while (0)

#define CHECK_THROWS(expr)                                                  \
    do {                                                                    \
        g_checks++;                                                         \
        bool thrown_ = false;                                               \
        try { expr; } catch ( ... ) { thrown_ = true; }                     \
        if ( !thrown_ ) {                                                   \
            g_failures++;                                                   \
            std::cerr << __FILE__ << ":" << __LINE__                        \
                      << ": expected exception: " << #expr << std::endl;    \
        }                                                                   \
    } while (0)

#define CHECK_NOTHROW(expr)                                                 \
    do {                                                                    \
        g_checks++;                                                         \
        try { expr; }                                                       \
        catch ( const std::exception& e_ ) {                                \
            g_failures++;                                                   \
            std::cerr << __FILE__ << ":" << __LINE__ << ": unexpected "     \
                      << "exception: " << e_.what() << std::endl;           \
        }                                                                   \
    } while (0)


static fs::path g_tmpDir;

static fs::path WriteTempFile(const std::string& name, const std::string& data)
{
    const fs::path p = g_tmpDir / name;
    std::ofstream f(p, std::ios::binary);
    f << data;
    return p;
}


/*--------------------------------------------------------------------------*/

static void TestBase64()
{
    CHECK(Base64Encode("", 0) == "");
    CHECK(Base64Encode("f", 1) == "Zg==");
    CHECK(Base64Encode("fo", 2) == "Zm8=");
    CHECK(Base64Encode("foo", 3) == "Zm9v");
    CHECK(Base64Encode("foobar", 6) == "Zm9vYmFy");

    CHECK(Base64Decode("Zm9vYmFy") == "foobar");
    CHECK(Base64Decode("Zm9v\r\nYmE=") == "fooba");
    CHECK(Base64Decode("Zg==") == "f");
    CHECK_THROWS(Base64Decode("Zm9v!"));
    CHECK_THROWS(Base64Decode("Z"));
    CHECK_THROWS(Base64Decode("Zg==Zg"));

    std::string all;
    for ( int i = 0; i < 256; i++ )
        all += char(i);
    CHECK(Base64Decode(Base64Encode(all.data(), all.size())) == all);
}


static void TestUtf8()
{
    const std::wstring w = L"naïve 中文 \U0001F600";
    const std::string u = ToUtf8(w);
    CHECK(u == "na\xC3\xAFve \xE4\xB8\xAD\xE6\x96\x87 \xF0\x9F\x98\x80");
    CHECK(FromUtf8(u) == w);
    CHECK(FromUtf8("\xFF" "a") == L"�" L"a");
    CHECK(FromUtf8("abc\xE4\xB8") == L"abc�");
}


static void TestVersionCompare()
{
    CHECK(UpdateChecker::CompareVersions("1.0", "1.0") == 0);
    CHECK(UpdateChecker::CompareVersions("1.0", "1.1") < 0);
    CHECK(UpdateChecker::CompareVersions("1.10", "1.9") > 0);
    CHECK(UpdateChecker::CompareVersions("1.5", "1.5b3") > 0);
    CHECK(UpdateChecker::CompareVersions("1.5.1", "1.5") > 0);
    CHECK(UpdateChecker::CompareVersions("1.2.0", "1.2rc1") > 0);
    CHECK(UpdateChecker::CompareVersions("1.2rc1", "1.2rc2") < 0);
    CHECK(UpdateChecker::CompareVersions("2.0", "10.0") < 0);
}


static void TestAppcast()
{
#if defined(_WIN32)
    const std::string os = "windows";
#elif defined(__APPLE__)
    const std::string os = "macos";
#else
    const std::string os = "linux";
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    const std::string arch = os + "-arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    const std::string arch = os + "-x64";
#else
    const std::string arch = os + "-x86";
#endif

    const std::string xml =
        "<?xml version='1.0' encoding='utf-8'?>"
        "<rss version='2.0' xmlns:sparkle='http://www.andymatuschak.org/xml-namespaces/sparkle'>"
        "<channel><title>Test</title>"
        // 1: only for some other OS -> skipped
        "<item><title>Other OS</title><sparkle:version>9.0</sparkle:version>"
        "  <enclosure url='https://example.com/other.bin' sparkle:os='amigaos'/></item>"
        // 2: generic + arch-specific enclosure -> arch-specific wins
        "<item><title>Version 2.0</title>"
        "  <sparkle:version>2.0</sparkle:version>"
        "  <sparkle:shortVersionString>2.0 Final</sparkle:shortVersionString>"
        "  <sparkle:releaseNotesLink> https://example.com/notes.html </sparkle:releaseNotesLink>"
        "  <sparkle:criticalUpdate/>"
        "  <enclosure url='https://example.com/generic.bin' sparkle:os='" + os + "' sparkle:edSignature='GEN'/>"
        "  <enclosure url='https://example.com/arch.bin' sparkle:os='" + arch + "' sparkle:edSignature='ARCH'"
        "             sparkle:installerArguments='/S --quiet'/>"
        "</item>"
        // 3: no enclosure, only link
        "<item><title>Version 1.5</title><sparkle:version>1.5</sparkle:version>"
        "  <link>https://example.com/download</link>"
        "  <description><![CDATA[<h1>Notes</h1>]]></description></item>"
        "</channel></rss>";

    std::vector<Appcast> items;
    CHECK_NOTHROW(items = Appcast::Load(xml));
    CHECK(items.size() == 2);
    if ( items.size() == 2 )
    {
        CHECK(items[0].Version == "2.0");
        CHECK(items[0].ShortVersionString == "2.0 Final");
        CHECK(items[0].ReleaseNotesURL == "https://example.com/notes.html");
        CHECK(items[0].CriticalUpdate);
        CHECK(items[0].enclosure.DownloadURL == "https://example.com/arch.bin");
        CHECK(items[0].enclosure.EdDsaSignature == "ARCH");
        CHECK(items[0].enclosure.InstallerArguments == "/S --quiet");

        CHECK(items[1].Version == "1.5");
        CHECK(!items[1].HasDownload());
        CHECK(items[1].WebBrowserURL == "https://example.com/download");
        CHECK(items[1].Description == "<h1>Notes</h1>");
    }

#ifndef __linux__
    // minimum OS version far in the future -> not applicable
    const std::string xml2 =
        "<rss xmlns:sparkle='http://www.andymatuschak.org/xml-namespaces/sparkle'><channel>"
        "<item><sparkle:version>3.0</sparkle:version>"
        "  <sparkle:minimumSystemVersion>999.0</sparkle:minimumSystemVersion>"
        "  <enclosure url='https://example.com/x.bin'/></item>"
        "<item><sparkle:version>2.5</sparkle:version>"
        "  <sparkle:minimumSystemVersion>1.0</sparkle:minimumSystemVersion>"
        "  <enclosure url='https://example.com/y.bin'/></item>"
        "</channel></rss>";
    CHECK_NOTHROW(items = Appcast::Load(xml2));
    CHECK(items.size() == 1 && items[0].Version == "2.5");
#endif

    CHECK_THROWS(Appcast::Load("<rss><channel><item>"));
}


static void TestDownloadHelpers()
{
    using namespace winsparkle::detail;

    CHECK(ParseContentDispositionFilename("attachment; filename=\"setup 1.0.exe\"") == "setup 1.0.exe");
    CHECK(ParseContentDispositionFilename("attachment; filename=setup.exe") == "setup.exe");
    CHECK(ParseContentDispositionFilename("attachment; filename=\"a;b.exe\"; size=3") == "a;b.exe");
    CHECK(ParseContentDispositionFilename("attachment; filename=\"fallback.exe\"; filename*=UTF-8''na%C3%AFve.exe")
          == "na\xC3\xAFve.exe");
    CHECK(ParseContentDispositionFilename("inline") == "");
    CHECK(ParseContentDispositionFilename("") == "");

    CHECK(GetURLFileName("https://example.com/dl/My%20App-1.0.dmg?token=abc#x") == "My App-1.0.dmg");
    CHECK(GetURLFileName("https://example.com") == "");
    CHECK(GetURLFileName("https://example.com/") == "");

    CHECK(SanitizeFileName(L"../../etc/passwd") == L"passwd");
    CHECK(SanitizeFileName(L"..\\..\\evil.exe") == L"evil.exe");
    CHECK(SanitizeFileName(L"..") == L"update");
    CHECK(SanitizeFileName(L"") == L"update");
    CHECK(SanitizeFileName(L"a:b?.exe") == L"a_b_.exe");
    CHECK(SanitizeFileName(L".hidden") == L"hidden");
    CHECK(SanitizeFileName(L"setup.exe. ") == L"setup.exe");
}


static void TestEmbeddedCABundle()
{
    size_t size = 0;
    const unsigned char *data = detail::GetEmbeddedCABundle(&size);
    CHECK(data != NULL);
    CHECK(size > 1000);
    CHECK(data[size] == 0); // NUL-terminated

    const std::string pem(reinterpret_cast<const char*>(data), size);
    size_t count = 0;
    for ( size_t pos = pem.find("-----BEGIN CERTIFICATE-----");
          pos != std::string::npos;
          pos = pem.find("-----BEGIN CERTIFICATE-----", pos + 1) )
    {
        count++;
    }
    CHECK(count > 50);
    CHECK(pem.find("-----END CERTIFICATE-----") != std::string::npos);
    // the human-readable dumps in cert.pem are stripped
    CHECK(pem.find("Signature Algorithm") == std::string::npos);
    std::cout << "  embedded CA bundle: " << count << " certificates, " << size << " bytes" << std::endl;
}


static void TestEdDSA()
{
    unsigned char seed[32], pub[32], priv[64];
    CHECK(ed25519_create_seed(seed) == 0);
    ed25519_create_keypair(pub, priv, seed);

    const std::string payload = "This is the update payload.\n";
    const fs::path file = WriteTempFile("update-eddsa.bin", payload);

    unsigned char sig[64];
    ed25519_sign(sig, reinterpret_cast<const unsigned char*>(payload.data()), payload.size(), pub, priv);

    const std::string pubB64 = Base64Encode(pub, sizeof(pub));
    const std::string sigB64 = Base64Encode(sig, sizeof(sig));

    CHECK(win_sparkle_set_eddsa_public_key("invalid") == 0);
    CHECK(win_sparkle_set_eddsa_public_key(pubB64.c_str()) == 1);
    CHECK(Settings::HasEdDSAPubKey());

    CHECK_NOTHROW(SignatureVerifier::VerifyEdDSASignatureValid(PathToWide(file), sigB64));

    // tampered file
    const fs::path bad = WriteTempFile("update-eddsa-bad.bin", payload + "x");
    CHECK_THROWS(SignatureVerifier::VerifyEdDSASignatureValid(PathToWide(bad), sigB64));
    // missing / malformed signature
    CHECK_THROWS(SignatureVerifier::VerifyEdDSASignatureValid(PathToWide(file), ""));
    CHECK_THROWS(SignatureVerifier::VerifyEdDSASignatureValid(PathToWide(file), "AAAA"));
}


#if WXSPARKLE_HAS_OPENSSL && OPENSSL_VERSION_NUMBER >= 0x30000000L
static void TestDSA()
{
    // generate a DSA key
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "DSA", NULL);
    EVP_PKEY *params = NULL;
    CHECK(pctx && EVP_PKEY_paramgen_init(pctx) == 1);
    CHECK(EVP_PKEY_CTX_set_dsa_paramgen_bits(pctx, 2048) == 1);
    CHECK(EVP_PKEY_paramgen(pctx, &params) == 1);
    EVP_PKEY_CTX_free(pctx);

    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_from_pkey(NULL, params, NULL);
    EVP_PKEY *key = NULL;
    CHECK(kctx && EVP_PKEY_keygen_init(kctx) == 1);
    CHECK(EVP_PKEY_keygen(kctx, &key) == 1);
    EVP_PKEY_CTX_free(kctx);
    EVP_PKEY_free(params);
    if ( !key )
        return;

    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, key);
    char *pemData = NULL;
    const long pemLen = BIO_get_mem_data(bio, &pemData);
    const std::string pem(pemData, size_t(pemLen));
    BIO_free(bio);

    // sign SHA1(SHA1(file)) like legacy Sparkle's sign_update script
    const std::string payload = "DSA-signed update payload";
    const fs::path file = WriteTempFile("update-dsa.bin", payload);
    unsigned char sha1[EVP_MAX_MD_SIZE], sha1sha1[EVP_MAX_MD_SIZE];
    unsigned int len1 = 0, len2 = 0;
    EVP_Digest(payload.data(), payload.size(), sha1, &len1, EVP_sha1(), NULL);
    EVP_Digest(sha1, len1, sha1sha1, &len2, EVP_sha1(), NULL);

    EVP_PKEY_CTX *sctx = EVP_PKEY_CTX_new(key, NULL);
    CHECK(EVP_PKEY_sign_init(sctx) == 1);
    size_t siglen = 0;
    EVP_PKEY_sign(sctx, NULL, &siglen, sha1sha1, len2);
    std::vector<unsigned char> sig(siglen);
    CHECK(EVP_PKEY_sign(sctx, sig.data(), &siglen, sha1sha1, len2) == 1);
    sig.resize(siglen);
    EVP_PKEY_CTX_free(sctx);
    EVP_PKEY_free(key);

    CHECK_NOTHROW(SignatureVerifier::VerifyDSAPubKeyPem(pem));
    CHECK_THROWS(SignatureVerifier::VerifyDSAPubKeyPem("-----BEGIN PUBLIC KEY-----\nAAAA\n-----END PUBLIC KEY-----\n"));

    CHECK_NOTHROW(Settings::SetDSAPubKeyPem(pem));
    const std::string sigB64 = Base64Encode(sig.data(), sig.size());
    CHECK_NOTHROW(SignatureVerifier::VerifyDSASHA1SignatureValid(PathToWide(file), sigB64));

    const fs::path bad = WriteTempFile("update-dsa-bad.bin", payload + "!");
    CHECK_THROWS(SignatureVerifier::VerifyDSASHA1SignatureValid(PathToWide(bad), sigB64));
}
#endif


static void TestSettingsStorage()
{
#ifndef _WIN32
    // default INI file based storage
    const fs::path ini = g_tmpDir / "config" / "settings.ini";
    win_sparkle_set_registry_path(ini.string().c_str());

    Settings::WriteConfigValue("Name", std::string("value"));
    Settings::WriteConfigValue("Multi", std::wstring(L"line1\nline2 \\ é"));
    Settings::WriteConfigValue("LastCheckTime", time_t(1234567890));
    Settings::WriteConfigValue("CheckForUpdates", true);

    std::string s;
    CHECK(Settings::ReadConfigValue("Name", s) && s == "value");
    std::wstring w;
    CHECK(Settings::ReadConfigValue("Multi", w) && w == L"line1\nline2 \\ é");
    time_t t = 0;
    CHECK(Settings::ReadConfigValue("LastCheckTime", t) && t == 1234567890);
    bool b = false;
    CHECK(Settings::ReadConfigValue("CheckForUpdates", b) && b);
    CHECK(win_sparkle_get_automatic_check_for_updates() == 1);

    Settings::WriteConfigValue("Name", std::string("other"));
    CHECK(Settings::ReadConfigValue("Name", s) && s == "other");

    Settings::DeleteConfigValue("Name");
    CHECK(!Settings::ReadConfigValue("Name", s));
    CHECK(fs::exists(ini));

    win_sparkle_set_update_check_interval(7200);
    CHECK(win_sparkle_get_update_check_interval() == 7200);
#endif

    // custom config methods
    static std::map<std::string, std::wstring> s_store;
    win_sparkle_config_methods_t methods;
    methods.config_read = [](const char *name, wchar_t *buf, size_t len, void *) -> int
    {
        auto it = s_store.find(name);
        if ( it == s_store.end() )
            return 0;
        wcsncpy(buf, it->second.c_str(), len);
        buf[len - 1] = 0;
        return 1;
    };
    methods.config_write = [](const char *name, const wchar_t *value, void *) { s_store[name] = value; };
    methods.config_delete = NULL; // use the default
    methods.user_data = NULL;
    win_sparkle_set_config_methods(&methods);

    Settings::WriteConfigValue("Custom", std::string("yes"));
    std::string s2;
    CHECK(Settings::ReadConfigValue("Custom", s2) && s2 == "yes");
    CHECK(s_store.count("Custom") == 1);

    win_sparkle_set_config_methods(NULL);
}


class TestThread : public Thread
{
public:
    TestThread(std::atomic<int>& counter, bool joinable)
        : Thread("test thread"), m_counter(counter), m_joinable(joinable) {}

protected:
    void Run() override
    {
        SignalReady();
        if ( m_joinable )
        {
            while ( !WaitForTermination(10) )
                m_counter++;
        }
        m_counter += 100;
    }

    bool IsJoinable() const override { return m_joinable; }

private:
    std::atomic<int>& m_counter;
    bool m_joinable;
};

static void TestThreads()
{
    std::atomic<int> counter(0);

    TestThread *t = new TestThread(counter, true);
    t->Start();
    t->TerminateAndJoin();
    delete t;
    CHECK(counter == 100);

    // self-destructing threads
    for ( int i = 0; i < 20; i++ )
        (new TestThread(counter, false))->Start();
    for ( int i = 0; i < 200 && counter < 2100; i++ )
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(counter == 2100);

    Event ev;
    CHECK(!ev.WaitUntilSignaled(10));
    ev.Signal();
    CHECK(ev.WaitUntilSignaled(10));
    CHECK(!ev.CheckIfSignaled()); // auto-reset
}


static void TestNetwork()
{
    // valid certificate, verified using the embedded CA bundle
    {
        StringDownloadSink sink;
        CHECK_NOTHROW(DownloadFile("https://curl.se/", &sink, NULL));
        CHECK(sink.data.find("curl") != std::string::npos);
    }
    // HTTP error
    {
        StringDownloadSink sink;
        CHECK_THROWS(DownloadFile("https://curl.se/this-page-does-not-exist-wxsparkle", &sink, NULL));
    }
    // invalid certificates must be rejected
    {
        StringDownloadSink sink;
        CHECK_THROWS(DownloadFile("https://expired.badssl.com/", &sink, NULL));
        CHECK_THROWS(DownloadFile("https://self-signed.badssl.com/", &sink, NULL));
        CHECK_THROWS(DownloadFile("https://wrong.host.badssl.com/", &sink, NULL));
    }
    // file name detection
    {
        struct NameSink : public StringDownloadSink
        {
            void SetFilename(const std::wstring& fn) override { name = fn; }
            void SetLength(size_t l) override { length = l; }
            std::wstring name;
            size_t length = 0;
        } sink;
        CHECK_NOTHROW(DownloadFile("https://curl.se/ca/cacert.pem", &sink, NULL, "", Download_BypassProxies));
        CHECK(sink.name == L"cacert.pem");
        CHECK(sink.data.find("BEGIN CERTIFICATE") != std::string::npos);
    }
}


int main(int argc, char **argv)
{
    wxInitializer wxinit(argc, argv);
    if ( !wxinit.IsOk() )
    {
        std::cerr << "Failed to initialize wxWidgets" << std::endl;
        return 2;
    }

    g_tmpDir = fs::temp_directory_path() / ("wxsparkle-tests-" + std::to_string(rand()));
    fs::create_directories(g_tmpDir);

    win_sparkle_set_app_details(L"wxSparkle", L"wxSparkle Tests", L"1.0");

    struct { const char *name; void (*fn)(); } tests[] =
    {
        { "base64", TestBase64 },
        { "utf8", TestUtf8 },
        { "version compare", TestVersionCompare },
        { "appcast", TestAppcast },
        { "download helpers", TestDownloadHelpers },
        { "embedded CA bundle", TestEmbeddedCABundle },
        { "EdDSA", TestEdDSA },
#if WXSPARKLE_HAS_OPENSSL && OPENSSL_VERSION_NUMBER >= 0x30000000L
        { "DSA", TestDSA },
#endif
        { "settings storage", TestSettingsStorage },
        { "threads", TestThreads },
    };

    for ( const auto& t : tests )
    {
        std::cout << "Running " << t.name << " tests..." << std::endl;
        try
        {
            t.fn();
        }
        catch ( const std::exception& e )
        {
            g_failures++;
            std::cerr << "  unexpected exception: " << e.what() << std::endl;
        }
    }

    const char *net = getenv("WXSPARKLE_TEST_NETWORK");
    if ( net && *net && strcmp(net, "0") != 0 )
    {
        std::cout << "Running network tests..." << std::endl;
        try
        {
            TestNetwork();
        }
        catch ( const std::exception& e )
        {
            g_failures++;
            std::cerr << "  unexpected exception: " << e.what() << std::endl;
        }
    }
    else
    {
        std::cout << "Skipping network tests (set WXSPARKLE_TEST_NETWORK=1 to run them)" << std::endl;
    }

    std::error_code ec;
    fs::remove_all(g_tmpDir, ec);

    std::cout << g_checks << " checks, " << g_failures << " failures" << std::endl;
    return g_failures ? 1 : 0;
}
