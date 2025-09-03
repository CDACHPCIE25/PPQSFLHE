#include "openfhe.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/ckksrns/ckksrns-ser.h"  // <-- This is MANDATORY for (de)serialization
//#include <nlohmann/json.hpp>
//#include <fstream>
#include <iostream>
#include <string>

using namespace lbcrypto;
//using json = nlohmann::json;
//TODO: Arg: CC
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] 
                  << " <cc_path> <pubkey_out> <privkey_out>" << std::endl;
        return 1;
    }

    std::string cc_path     = argv[1];
    std::string pubkey_out  = argv[2];
    std::string privkey_out = argv[3];

    // Step 1: Load CryptoContext
    CryptoContext<DCRTPoly> cc;
    if (!Serial::DeserializeFromFile(cc_path, cc, SerType::JSON)) {
        std::cerr << "[keyGen] ERROR: cannot load CryptoContext from " << cc_path << std::endl;
        return 1;
    }
    std::cout << "[keyGen] CryptoContext loaded from " << cc_path << std::endl;

    // Step 2: Generate KeyPair
    KeyPair<DCRTPoly> keyPair = cc->KeyGen();
    if (!keyPair.good()) {
        std::cerr << "[keyGen] ERROR: Key generation failed" << std::endl;
        return 1;
    }
    std::cout << "[keyGen] Public and Private keys generated" << std::endl;

    // Step 3: Serialize Keys
    if (!Serial::SerializeToFile(privkey_out, keyPair.secretKey, SerType::JSON)) {
        std::cerr << "[keyGen] ERROR: Failed to save private key to " << privkey_out << std::endl;
        return 1;
    }
    if (!Serial::SerializeToFile(pubkey_out, keyPair.publicKey, SerType::JSON)) {
        std::cerr << "[keyGen] ERROR: Failed to save public key to " << pubkey_out << std::endl;
        return 1;
    }

    std::cout << "[keyGen] Keys saved: priv=" << privkey_out 
              << " pub=" << pubkey_out << std::endl;
    return 0;
}
