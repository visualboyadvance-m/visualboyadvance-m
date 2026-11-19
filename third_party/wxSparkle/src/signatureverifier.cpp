/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "signatureverifier.h"

#include "error.h"
#include "settings.h"
#include "utils.h"

#if WXSPARKLE_HAS_OPENSSL
    #include <openssl/err.h>
    #include <openssl/evp.h>
    #include <openssl/pem.h>
#endif

#include <ed25519.h>

#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace winsparkle
{

namespace
{

std::vector<unsigned char> ReadWholeFile(const std::wstring& filename)
{
    std::ifstream f(PathFromWide(filename), std::ios::binary | std::ios::ate);
    if ( !f )
        throw std::runtime_error("Failed to open file " + ToUtf8(filename));

    const std::streamoff size = f.tellg();
    if ( size < 0 )
        throw std::runtime_error("Failed to read file " + ToUtf8(filename));
    f.seekg(0, std::ios::beg);

    std::vector<unsigned char> payload(static_cast<size_t>(size));
    if ( size > 0 && !f.read(reinterpret_cast<char*>(payload.data()), size) )
        throw std::runtime_error("Failed to read file " + ToUtf8(filename));

    return payload;
}


#if WXSPARKLE_HAS_OPENSSL

std::string GetOpenSSLError()
{
    char buf[256];
    const unsigned long err = ERR_get_error();
    if ( !err )
        return "unknown OpenSSL error";
    ERR_error_string_n(err, buf, sizeof(buf));
    return buf;
}

struct EVP_PKEY_Deleter     { void operator()(EVP_PKEY *p) const     { EVP_PKEY_free(p); } };
struct EVP_PKEY_CTX_Deleter { void operator()(EVP_PKEY_CTX *p) const { EVP_PKEY_CTX_free(p); } };
struct EVP_MD_CTX_Deleter   { void operator()(EVP_MD_CTX *p) const   { EVP_MD_CTX_free(p); } };
struct BIO_Deleter          { void operator()(BIO *p) const          { BIO_free(p); } };

typedef std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter> EVP_PKEY_ptr;

// Loads DSA public key from PEM; throws if it isn't a valid DSA public key.
EVP_PKEY_ptr LoadDSAPubKey(const std::string& pem)
{
    std::unique_ptr<BIO, BIO_Deleter> bio(BIO_new_mem_buf(pem.data(), int(pem.size())));
    if ( !bio )
        throw std::invalid_argument("Cannot set PEM key mem buffer");

    EVP_PKEY_ptr key(PEM_read_bio_PUBKEY(bio.get(), NULL, NULL, NULL));
    if ( !key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_DSA )
        throw std::invalid_argument("Cannot read DSA public key from PEM");

    return key;
}

class SHA1Hash
{
public:
    SHA1Hash() : m_ctx(EVP_MD_CTX_new())
    {
        if ( !m_ctx || EVP_DigestInit_ex(m_ctx.get(), EVP_sha1(), NULL) != 1 )
            throw std::runtime_error("Failed to create crypto hash: " + GetOpenSSLError());
    }

    void hashData(const void *buffer, size_t len)
    {
        if ( EVP_DigestUpdate(m_ctx.get(), buffer, len) != 1 )
            throw std::runtime_error("Failed to hash data: " + GetOpenSSLError());
    }

    void hashFile(const std::wstring& filename)
    {
        std::ifstream f(PathFromWide(filename), std::ios::binary);
        if ( !f )
            throw std::runtime_error("Failed to open file " + ToUtf8(filename));

        char buf[8192];
        while ( f )
        {
            f.read(buf, sizeof(buf));
            const std::streamsize n = f.gcount();
            if ( n > 0 )
                hashData(buf, static_cast<size_t>(n));
        }
        if ( f.bad() )
            throw std::runtime_error("Failed to read file " + ToUtf8(filename));
    }

    std::vector<unsigned char> value()
    {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        if ( EVP_DigestFinal_ex(m_ctx.get(), md, &len) != 1 )
            throw std::runtime_error("Failed to get SHA1 val: " + GetOpenSSLError());
        return std::vector<unsigned char>(md, md + len);
    }

private:
    std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_Deleter> m_ctx;
};

void VerifyDSASHA1Signature(const std::wstring& filename, const std::string& signature)
{
    // SHA1 of SHA1 of file
    std::vector<unsigned char> sha1;
    {
        SHA1Hash hash;
        hash.hashFile(filename);
        sha1 = hash.value();
    }
    {
        SHA1Hash hash;
        hash.hashData(sha1.data(), sha1.size());
        sha1 = hash.value();
    }

    EVP_PKEY_ptr pubKey = LoadDSAPubKey(Settings::GetDSAPubKeyPem());

    std::unique_ptr<EVP_PKEY_CTX, EVP_PKEY_CTX_Deleter> ctx(EVP_PKEY_CTX_new(pubKey.get(), NULL));
    if ( !ctx || EVP_PKEY_verify_init(ctx.get()) != 1 )
        throw BadSignatureException(GetOpenSSLError());

    // The signed data is the raw digest (no message digest is set on the
    // context), which is equivalent to legacy DSA_verify(0, sha1, ...).
    const int code = EVP_PKEY_verify(ctx.get(),
                                     reinterpret_cast<const unsigned char*>(signature.data()), signature.size(),
                                     sha1.data(), sha1.size());

    if ( code < 0 ) // OpenSSL error
        throw BadSignatureException(GetOpenSSLError());

    if ( code != 1 )
        throw BadSignatureException();
}

#endif // WXSPARKLE_HAS_OPENSSL

} // anonynous

void SignatureVerifier::VerifyDSAPubKeyPem(const std::string &pem)
{
#if WXSPARKLE_HAS_OPENSSL
    // throws if not valid
    LoadDSAPubKey(pem);
#else
    (void)pem;
    throw std::runtime_error("DSA signatures are not supported by this build of wxSparkle (built without OpenSSL); use EdDSA.");
#endif
}

void SignatureVerifier::VerifyEdDSAPubKey(const std::string& pubkey_base64)
{
    const std::string pubkey = Base64Decode(pubkey_base64);
    if (pubkey.size() != 32)
    {
        throw BadSignatureException("Invalid public key size.");
    }
}

void SignatureVerifier::VerifyDSASHA1SignatureValid(const std::wstring &filename, const std::string &signature_base64)
{
    try
    {
        if (signature_base64.size() == 0)
            throw BadSignatureException("Missing DSA signature!");
#if WXSPARKLE_HAS_OPENSSL
        VerifyDSASHA1Signature(filename, Base64Decode(signature_base64));
#else
        (void)filename;
        throw BadSignatureException("DSA signatures are not supported by this build");
#endif
    }
    catch (BadSignatureException&)
    {
        throw;
    }
    catch (const std::exception &e)
    {
        throw BadSignatureException(e.what());
    }
    catch (...)
    {
        throw BadSignatureException();
    }
}

void SignatureVerifier::VerifyEdDSASignatureValid(const std::wstring& filename, const std::string& signature_base64)
{
    if (signature_base64.size() == 0)
        throw BadSignatureException("Missing EdDSA signature!");

    const std::vector<unsigned char> payload = ReadWholeFile(filename);

    const std::string signature = Base64Decode(signature_base64);
    if (signature.size() != 64)
    {
        throw BadSignatureException("Invalid signature size.");
    }

    const std::string pubkey = Base64Decode(Settings::GetEdDSAPubKey());
    if (pubkey.size() != 32)
    {
        throw BadSignatureException("Invalid public key size.");
    }

    int result = ed25519_verify(reinterpret_cast<const unsigned char*>(signature.data()),
                                payload.data(),
                                payload.size(),
                                reinterpret_cast<const unsigned char*>(pubkey.data()));
    if (result != 1)
        throw BadSignatureException();
}

} // namespace winsparkle
