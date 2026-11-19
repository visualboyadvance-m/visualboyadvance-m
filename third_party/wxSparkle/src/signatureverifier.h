/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _signatureverifier_h_
#define _signatureverifier_h_

#include <stdexcept>
#include <string>

namespace winsparkle
{

class BadSignatureException : public std::runtime_error
{
public:
    BadSignatureException() : std::runtime_error("Invalid update signature") {}
    BadSignatureException(const std::string& msg) : std::runtime_error("Invalid update signature: " + msg) {}
};

class SignatureVerifier
{
public:
    // Throws an exception if pem is not a valid DSA public key in PEM format
    static void VerifyDSAPubKeyPem(const std::string &pem);

    // Throws an exception if pem is not a valid EdDSA public key in base64 format
    static void VerifyEdDSAPubKey(const std::string& pubkey_base64);

    // Verify DSA signature of SHA1 hash of the file. Equivalent to:
    // openssl dgst -sha1 -binary < filename | openssl dgst -sha1 -verify dsa_pub.pem -signature signature.bin
    // Throws BadSignatureException on failure.
    static void VerifyDSASHA1SignatureValid(const std::wstring &filename, const std::string &signature_base64);

    // Verify EdDSA signature of the file.
    // Throws BadSignatureException on failure.
    static void VerifyEdDSASignatureValid(const std::wstring& filename, const std::string& signature_base64);
};

} // namespace winsparkle

#endif // _signatureverifier_h_
