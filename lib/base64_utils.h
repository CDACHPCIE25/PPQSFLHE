#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// Base64 Encode
inline std::string Base64Encode(const std::string& input) {
    BIO* bio, *b64;
    BUF_MEM* bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // no newlines
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, input.data(), input.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string encoded(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return encoded;
}

// Base64 Decode
inline std::string Base64Decode(const std::string& input) {
    BIO* bio, *b64;
    int decodeLen = input.size();
    std::string output(decodeLen, '\0');

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // no newlines
    bio = BIO_new_mem_buf(input.data(), input.size());
    bio = BIO_push(b64, bio);

    int length = BIO_read(bio, output.data(), input.size());
    BIO_free_all(bio);

    if (length < 0) {
        return ""; // decoding failed
    }

    output.resize(length); // trim extra nulls
    return output;
}

#endif // UTILS_H
