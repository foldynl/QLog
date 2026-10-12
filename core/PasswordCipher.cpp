#include <QDataStream>
#include <QIODevice>
#include <QLoggingCategory>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include "PasswordCipher.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.passwordcipher");

namespace {
  const char kMagic[4] = {'P','M','G','R'};
  const quint8 kVersion = 1;
  const quint8 kKdfPBKDF2 = 1;
  const quint8 kAeadAES256GCM = 1;

  inline QByteArray toUtf8Norm(const QString& s)
  {
    return s.toUtf8();
  }

  inline bool randFill(QByteArray& out)
  {
    return RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), out.size()) == 1;
  }

  inline void logOpensslError(const char* where)
  {
    unsigned long ec = ERR_get_error();
    if (ec != 0)
    {
      char buf[256] = {0};
      ERR_error_string_n(ec, buf, sizeof(buf));
      qCDebug(runtime) << where << "openssl:" << buf;
    }
    else
    {
      qCDebug(runtime) << where << "failed";
    }
  }
}

void PasswordCipher::cleanse(QByteArray& buf)
{
    FCT_IDENTIFICATION;
    if (!buf.isEmpty()) OPENSSL_cleanse(buf.data(), static_cast<size_t>(buf.size()));
}

bool PasswordCipher::kdf_pbkdf2_sha256(const QByteArray& passUtf8,
                                       const QByteArray& salt,
                                       int iters,
                                       QByteArray& outKey32)
{
    FCT_IDENTIFICATION;

    outKey32.resize(kKeyLen);
    int ok = PKCS5_PBKDF2_HMAC(passUtf8.constData(), passUtf8.size(),
                               reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
                               iters, EVP_sha256(),
                               outKey32.size(),
                               reinterpret_cast<unsigned char*>(outKey32.data()));
    if (ok != 1)
    {
        logOpensslError("PKCS5_PBKDF2_HMAC");
        outKey32.clear();
        return false;
    }
    return true;
}

bool PasswordCipher::aes_gcm_encrypt(const QByteArray& key,
                                     const QByteArray& iv,
                                     const QByteArray& pt,
                                     QByteArray& ct,
                                     QByteArray& tag)
{
    FCT_IDENTIFICATION;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if ( !ctx )
    {
        logOpensslError("EVP_CIPHER_CTX_new");
        return false;
    }

    bool ok = false;
    do
    {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        {
            logOpensslError("EncryptInit");
            break;
        }

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1)
        {
            logOpensslError("SET_IVLEN");
            break;
        }

        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                               reinterpret_cast<const unsigned char*>(key.constData()),
                               reinterpret_cast<const unsigned char*>(iv.constData())) != 1)
        {
            logOpensslError("EncryptInit key/iv");
            break;
        }

        ct.resize(pt.size());
        int outl = 0, total = 0;
        if (EVP_EncryptUpdate(ctx,
                              reinterpret_cast<unsigned char*>(ct.data()), &outl,
                              reinterpret_cast<const unsigned char*>(pt.constData()), pt.size()) != 1)
        {
            logOpensslError("EncryptUpdate");
            break;
        }

        total += outl;

        if (EVP_EncryptFinal_ex(ctx,
                                reinterpret_cast<unsigned char*>(ct.data()) + total, &outl) != 1)
        {
            logOpensslError("EncryptFinal");
            break;
        }

        total += outl; ct.resize(total);

        tag.resize(kTagLen);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1)
        {
            logOpensslError("GET_TAG");
            break;
        }

        ok = true;
    } while(false);

    if (!ok)
    {
        cleanse(ct);
        cleanse(tag);
    }

    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

bool PasswordCipher::aes_gcm_decrypt(const QByteArray& key,
                                     const QByteArray& iv,
                                     const QByteArray& ct,
                                     const QByteArray& tag,
                                     QByteArray& pt)
{
    FCT_IDENTIFICATION;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if ( !ctx )
    {
        logOpensslError("EVP_CIPHER_CTX_new");
        return false;
    }

    bool ok = false;
    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        {
            logOpensslError("DecryptInit");
            break;
        }

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1)
        {
            logOpensslError("SET_IVLEN");
            break;
        }

        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                               reinterpret_cast<const unsigned char*>(key.constData()),
                               reinterpret_cast<const unsigned char*>(iv.constData())) != 1)
        {
            logOpensslError("DecryptInit key/iv");
            break;
        }

        pt.resize(ct.size());
        int outl = 0, total = 0;
        if (EVP_DecryptUpdate(ctx,
                              reinterpret_cast<unsigned char*>(pt.data()), &outl,
                              reinterpret_cast<const unsigned char*>(ct.constData()), ct.size()) != 1)
        {
            logOpensslError("DecryptUpdate");
            break;
        }

        total += outl;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), const_cast<char*>(tag.constData())) != 1)
        {
            logOpensslError("SET_TAG");
            break;
        }

        int finok = EVP_DecryptFinal_ex(ctx,
                                        reinterpret_cast<unsigned char*>(pt.data()) + total, &outl);
        if (finok != 1)
        {
            // Auth failed or wrong key/passphrase/iv/tag
            pt.clear();
            logOpensslError("DecryptFinal (auth fail)");
            break;
        }
        total += outl; pt.resize(total);
        ok = true;
    } while(false);

    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

bool PasswordCipher::encrypt(const QString& passphrase,
                             const QByteArray& plaintext,
                             QByteArray& outBase64)
{
    FCT_IDENTIFICATION;

    outBase64.clear();

    if ( plaintext.isEmpty() ) return false;

    QByteArray pass = toUtf8Norm(passphrase);
    QByteArray salt(kSaltLen, 0);

    if (!randFill(salt))
    {
        logOpensslError("RAND salt");
        return false;
    }

    QByteArray key;

    if ( !kdf_pbkdf2_sha256(pass, salt, kPBKDF2Iters, key))
    {
        cleanse(pass);
        return false;
    }

    QByteArray iv(kIVLen, 0);

    if (!randFill(iv))
    {
        logOpensslError("RAND iv");
        cleanse(key);
        cleanse(pass);
        return false;
    }

    QByteArray ct, tag;

    if (!aes_gcm_encrypt(key, iv, plaintext, ct, tag))
    {
        cleanse(key);
        cleanse(pass);
        return false;
    }

    cleanse(pass);

    // Build blob
    QByteArray blob;
    blob.reserve(4+1+1+1+1+1+1+1+4 + salt.size() + iv.size() + ct.size() + tag.size());
    QDataStream ds(&blob, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    ds.writeRawData(kMagic, 4);
    ds << kVersion;
    ds << kKdfPBKDF2;
    ds << kAeadAES256GCM;
    ds << static_cast<quint8>(salt.size());
    ds << static_cast<quint8>(iv.size());
    ds << static_cast<quint8>(tag.size());
    ds << static_cast<quint8>(0); // reserved
    ds << static_cast<quint32>(kPBKDF2Iters);
    ds.writeRawData(salt.constData(), salt.size());
    ds.writeRawData(iv.constData(), iv.size());
    ds.writeRawData(ct.constData(), ct.size());
    ds.writeRawData(tag.constData(), tag.size());

    outBase64 = blob.toBase64();

    cleanse(key);
    return true;
}

bool PasswordCipher::decrypt(const QString& passphrase,
                             const QByteArray& inBase64,
                             QByteArray& plaintext)
{
    FCT_IDENTIFICATION;

    plaintext.clear();
    QByteArray blob = QByteArray::fromBase64(inBase64);
    if (blob.size() < (int)(4+1+1+1+1+1+1+1+4)) return false;

    QDataStream ds(blob);
    ds.setByteOrder(QDataStream::BigEndian);

    char mg[4];
    if (ds.readRawData(mg, 4) != 4) return false;
    if (memcmp(mg, kMagic, 4) != 0) return false;

    quint8 ver=0, kdf=0, aead=0, slen=0, ivlen=0, tlen=0, rsvd=0;
    quint32 iters=0;
    ds >> ver >> kdf >> aead >> slen >> ivlen >> tlen >> rsvd >> iters;

    if (ds.status()!=QDataStream::Ok) return false;
    if (ver != kVersion || kdf != kKdfPBKDF2 || aead != kAeadAES256GCM) return false;
    if ( slen == 0 || ivlen == 0 || tlen != kTagLen )
        return false;
    if (iters < 10000) return false; // sanity

    QByteArray salt(slen, 0), iv(ivlen, 0);
    if (ds.readRawData(salt.data(), slen) != slen) return false;
    if (ds.readRawData(iv.data(), ivlen) != ivlen) return false;

    // Remaining = ct + tag
    int remain = blob.size() - (4+1+1+1+1+1+1+1+4 + slen + ivlen);
    if (remain <= tlen) return false;
    int ctlen = remain - tlen;

    QByteArray ct(ctlen, 0), tag(tlen, 0);
    if (ds.readRawData(ct.data(), ctlen) != ctlen) return false;
    if (ds.readRawData(tag.data(), tlen) != tlen) return false;

    QByteArray pass = toUtf8Norm(passphrase);
    QByteArray key;
    if (!kdf_pbkdf2_sha256(pass, salt, static_cast<int>(iters), key)) { cleanse(pass); return false; }

    bool ok = aes_gcm_decrypt(key, iv, ct, tag, plaintext);

    cleanse(key);
    cleanse(pass);
    if (!ok) plaintext.clear();
    return ok;
}
