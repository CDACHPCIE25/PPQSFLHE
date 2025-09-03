#include "openfhe.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "key/key.h"
#include "scheme/ckksrns/ckksrns-pke.h"
#include "scheme/ckksrns/ckksrns-pre.h"
#include <fstream>
#include <iostream>
//#include <nlohmann/json.hpp>

using namespace lbcrypto;
//using json = nlohmann::json;

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] 
                  << " <cc.json> <client_privkey.json> <peer_pubkey.json> <rekey_out.json>" 
                  << std::endl;
        return 1;
    }

    std::string cc_path     = argv[1];
    std::string client_sk   = argv[2];
    std::string client_pk   = argv[3];
    std::string rekey_path  = argv[4];

    // Load CryptoContext
    CryptoContext<DCRTPoly> cc;
    if (!Serial::DeserializeFromFile(cc_path, cc, SerType::JSON)) {
        std::cerr << "Error loading CryptoContext from " << cc_path << std::endl;
        return 1;
    }
    std::cout << "[ReKeyGen] CryptoContext loaded from " << cc_path << std::endl;

    // Load Client Private Key
    PrivateKey<DCRTPoly> privKey;
    if (!Serial::DeserializeFromFile(client_sk, privKey, SerType::JSON)) {
        std::cerr << "Error loading Client private key from " << client_sk << std::endl;
        return 1;
    }
    std::cout << "[ReKeyGen] Client Private Key loaded from " << client_sk << std::endl;

    // Load Peer Public Key
    PublicKey<DCRTPoly> pubKey;
    if (!Serial::DeserializeFromFile(client_pk, pubKey, SerType::JSON)) {
        std::cerr << "Error loading Peer public key from " << client_pk << std::endl;
        return 1;
    }
    std::cout << "[ReKeyGen] Peer Public Key loaded from " << client_pk << std::endl;

    // Generate Re-Encryption Key
    EvalKey<DCRTPoly> reKey = cc->ReKeyGen(privKey, pubKey);
    if (!reKey) {
        std::cerr << "[ReKeyGen] Re-encryption key generation failed" << std::endl;
        return 1;
    }
    std::cout << "[ReKeyGen] Re-encryption key generated successfully" << std::endl;

    // Save Re-Encryption Key
    if (!Serial::SerializeToFile(rekey_path, reKey, SerType::JSON)) {
        std::cerr << "[ReKeyGen] Failed to save re-encryption key to " << rekey_path << std::endl;
        return 1;
    }
    std::cout << "[ReKeyGen] Re-encryption key saved to " << rekey_path << std::endl;

    return 0;
}
