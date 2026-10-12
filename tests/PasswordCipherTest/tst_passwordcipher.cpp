#include <QtTest>
#include <QRandomGenerator>

#include "core/PasswordCipher.h"

class PasswordCipherTest : public QObject
{
    Q_OBJECT

private slots:
    // Basic functionality
    void encrypt_emptyPlaintext_succeeds();
    void encrypt_smallData_encryptsAndDecrypts();
    void encrypt_largeData_encryptsAndDecrypts();
    void encrypt_binaryData_encryptsAndDecrypts();
    void encrypt_unicodePassword_works();
    void encrypt_longPassword_works();

    // Decryption failures
    void decrypt_wrongPassword_fails();
    void decrypt_invalidBase64_fails();
    void decrypt_truncatedData_fails();
    void decrypt_corruptedData_fails();
    void decrypt_modifiedCiphertext_fails();
    void decrypt_modifiedTag_fails();
    void decrypt_tagLength_data();
    void decrypt_tagLength();

    // Edge cases
    void encrypt_emptyPassword_succeeds();
    void encrypt_sameDataTwice_producesDifferentOutput();

    // Security properties
    void encrypt_outputIsBase64();
    void encrypt_outputContainsMagicHeader();

    // Benchmarks
    void encrypt_benchmark();
    void decrypt_benchmark();

private:
    QByteArray generateRandomData(int size);
};

QByteArray PasswordCipherTest::generateRandomData(int size)
{
    QByteArray data(size, '\0');
    for (int i = 0; i < size; ++i)
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return data;
}

// ============================================================================
// Basic functionality
// ============================================================================

void PasswordCipherTest::encrypt_emptyPlaintext_succeeds()
{
    // PasswordCipher intentionally rejects empty plaintext for security reasons
    QString password = "test-password";
    QByteArray plaintext;  // empty
    QByteArray encrypted;

    // Empty plaintext should be rejected
    QVERIFY(!PasswordCipher::encrypt(password, plaintext, encrypted));
    QVERIFY(encrypted.isEmpty());
}

void PasswordCipherTest::encrypt_smallData_encryptsAndDecrypts()
{
    QString password = "my-secret-password-123";
    QByteArray plaintext = "Hello, World! This is a secret message.";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));
    QVERIFY(!encrypted.isEmpty());
    QVERIFY(encrypted != plaintext);

    QByteArray decrypted;
    QVERIFY(PasswordCipher::decrypt(password, encrypted, decrypted));
    QCOMPARE(decrypted, plaintext);
}

void PasswordCipherTest::encrypt_largeData_encryptsAndDecrypts()
{
    QString password = "password-for-large-data";
    QByteArray plaintext = generateRandomData(1024 * 1024);  // 1 MB
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));
    QVERIFY(!encrypted.isEmpty());

    QByteArray decrypted;
    QVERIFY(PasswordCipher::decrypt(password, encrypted, decrypted));
    QCOMPARE(decrypted, plaintext);
}

void PasswordCipherTest::encrypt_binaryData_encryptsAndDecrypts()
{
    QString password = "binary-data-password";
    // Create binary data with all byte values including null bytes
    QByteArray plaintext;
    for (int i = 0; i < 256; ++i)
        plaintext.append(static_cast<char>(i));

    QByteArray encrypted;
    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    QByteArray decrypted;
    QVERIFY(PasswordCipher::decrypt(password, encrypted, decrypted));
    QCOMPARE(decrypted, plaintext);
}

void PasswordCipherTest::encrypt_unicodePassword_works()
{
    QString password = QString::fromUtf8("密码测试🔐");  // Chinese + emoji
    QByteArray plaintext = "Secret data with unicode password";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    QByteArray decrypted;
    QVERIFY(PasswordCipher::decrypt(password, encrypted, decrypted));
    QCOMPARE(decrypted, plaintext);
}

void PasswordCipherTest::encrypt_longPassword_works()
{
    // Very long password (1000 characters)
    QString password = QString("a").repeated(1000);
    QByteArray plaintext = "Data encrypted with very long password";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    QByteArray decrypted;
    QVERIFY(PasswordCipher::decrypt(password, encrypted, decrypted));
    QCOMPARE(decrypted, plaintext);
}

// ============================================================================
// Decryption failures
// ============================================================================

void PasswordCipherTest::decrypt_wrongPassword_fails()
{
    QString correctPassword = "correct-password";
    QString wrongPassword = "wrong-password";
    QByteArray plaintext = "Secret message";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(correctPassword, plaintext, encrypted));

    QByteArray decrypted;
    QVERIFY(!PasswordCipher::decrypt(wrongPassword, encrypted, decrypted));
}

void PasswordCipherTest::decrypt_invalidBase64_fails()
{
    QString password = "password";
    QByteArray invalidBase64 = "This is not valid base64!!!@@@";

    QByteArray decrypted;
    QVERIFY(!PasswordCipher::decrypt(password, invalidBase64, decrypted));
}

void PasswordCipherTest::decrypt_truncatedData_fails()
{
    QString password = "password";
    QByteArray plaintext = "Some data to encrypt";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    // Truncate the encrypted data
    QByteArray truncated = encrypted.left(encrypted.size() / 2);

    QByteArray decrypted;
    QVERIFY(!PasswordCipher::decrypt(password, truncated, decrypted));
}

void PasswordCipherTest::decrypt_corruptedData_fails()
{
    QString password = "password";
    QByteArray plaintext = "Data to be corrupted";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    // Decode, corrupt, re-encode
    QByteArray decoded = QByteArray::fromBase64(encrypted);
    if (decoded.size() > 20)
        decoded[20] = static_cast<char>(decoded[20] ^ 0xFF);  // Flip bits in the middle
    QByteArray corrupted = decoded.toBase64();

    QByteArray decrypted;
    QVERIFY(!PasswordCipher::decrypt(password, corrupted, decrypted));
}

void PasswordCipherTest::decrypt_modifiedCiphertext_fails()
{
    QString password = "password";
    QByteArray plaintext = "Original plaintext data";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    // Decode and modify ciphertext portion (after header)
    QByteArray decoded = QByteArray::fromBase64(encrypted);
    // Header is about 16 bytes + salt(16) + iv(12) = 44 bytes, then ciphertext starts
    if (decoded.size() > 50)
        decoded[50] = static_cast<char>(decoded[50] ^ 0xFF);
    QByteArray modified = decoded.toBase64();

    QByteArray decrypted;
    QVERIFY(!PasswordCipher::decrypt(password, modified, decrypted));
}

void PasswordCipherTest::decrypt_modifiedTag_fails()
{
    QString password = "password";
    QByteArray plaintext = "Data with protected integrity";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    // Decode and modify the last byte (part of tag)
    QByteArray decoded = QByteArray::fromBase64(encrypted);
    if (!decoded.isEmpty())
        decoded[decoded.size() - 1] = static_cast<char>(decoded[decoded.size() - 1] ^ 0xFF);
    QByteArray modified = decoded.toBase64();

    QByteArray decrypted;
    QVERIFY(!PasswordCipher::decrypt(password, modified, decrypted));
}

void PasswordCipherTest::decrypt_tagLength_data()
{
    QTest::addColumn<int>("tagLength");
    QTest::addColumn<bool>("valid");
    QTest::newRow("one-byte") << 1 << false;
    QTest::newRow("twelve-bytes") << 12 << false;
    QTest::newRow("fifteen-bytes") << 15 << false;
    QTest::newRow("full-tag") << 16 << true;
}

void PasswordCipherTest::decrypt_tagLength()
{
    QFETCH(int, tagLength);
    QFETCH(bool, valid);
    const QString password("password");
    const QByteArray plaintext("Data with protected integrity");
    QByteArray encrypted;
    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));
    QByteArray blob = QByteArray::fromBase64(encrypted);
    QCOMPARE(static_cast<quint8>(blob.at(9)), quint8(16));
    // The version-1 header stores the tag length at byte 9.
    blob[9] = static_cast<char>(tagLength);
    blob.chop(16 - tagLength);
    QByteArray decrypted("previous plaintext");
    QCOMPARE(PasswordCipher::decrypt(password, blob.toBase64(), decrypted), valid);
    QCOMPARE(decrypted, valid ? plaintext : QByteArray());
}

// ============================================================================
// Edge cases
// ============================================================================

void PasswordCipherTest::encrypt_emptyPassword_succeeds()
{
    QString password;  // empty
    QByteArray plaintext = "Data with empty password";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    QByteArray decrypted;
    QVERIFY(PasswordCipher::decrypt(password, encrypted, decrypted));
    QCOMPARE(decrypted, plaintext);
}

void PasswordCipherTest::encrypt_sameDataTwice_producesDifferentOutput()
{
    QString password = "same-password";
    QByteArray plaintext = "Same plaintext data";

    QByteArray encrypted1, encrypted2;
    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted1));
    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted2));

    // Due to random salt and IV, outputs should be different
    QVERIFY(encrypted1 != encrypted2);

    // But both should decrypt to the same plaintext
    QByteArray decrypted1, decrypted2;
    QVERIFY(PasswordCipher::decrypt(password, encrypted1, decrypted1));
    QVERIFY(PasswordCipher::decrypt(password, encrypted2, decrypted2));
    QCOMPARE(decrypted1, plaintext);
    QCOMPARE(decrypted2, plaintext);
}

// ============================================================================
// Security properties
// ============================================================================

void PasswordCipherTest::encrypt_outputIsBase64()
{
    QString password = "password";
    QByteArray plaintext = "Test data";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    // Verify it's valid base64 by decoding
    QByteArray decoded = QByteArray::fromBase64(encrypted, QByteArray::Base64Encoding);
    QVERIFY(!decoded.isEmpty());

    // Re-encode should match (possibly with padding differences)
    QByteArray reencoded = decoded.toBase64();
    QCOMPARE(reencoded, encrypted);
}

void PasswordCipherTest::encrypt_outputContainsMagicHeader()
{
    QString password = "password";
    QByteArray plaintext = "Test data";
    QByteArray encrypted;

    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    // Decode and check magic header
    QByteArray decoded = QByteArray::fromBase64(encrypted);
    QVERIFY(decoded.size() >= 4);
    QCOMPARE(decoded.left(4), QByteArray("PMGR"));
}

// ============================================================================
// Benchmarks
// ============================================================================

void PasswordCipherTest::encrypt_benchmark()
{
    QString password = "benchmark-password";
    QByteArray plaintext = generateRandomData(10000);  // 10 KB

    QByteArray encrypted;
    QBENCHMARK {
        PasswordCipher::encrypt(password, plaintext, encrypted);
    }
}

void PasswordCipherTest::decrypt_benchmark()
{
    QString password = "benchmark-password";
    QByteArray plaintext = generateRandomData(10000);  // 10 KB
    QByteArray encrypted;
    QVERIFY(PasswordCipher::encrypt(password, plaintext, encrypted));

    QByteArray decrypted;
    QBENCHMARK {
        PasswordCipher::decrypt(password, encrypted, decrypted);
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    PasswordCipherTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_passwordcipher.moc"
