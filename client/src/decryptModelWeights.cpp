#include "openfhe.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "ciphertext-ser.h"
#include "base64_utils.h"

using json = nlohmann::json;
using namespace lbcrypto;

// Decode Base64 -> Ciphertext
Ciphertext<DCRTPoly> decodeCiphertext(const std::string& b64) {
    std::string bin = Base64Decode(b64);
    std::stringstream ss(bin);
    Ciphertext<DCRTPoly> ct;
    Serial::Deserialize(ct, ss, SerType::BINARY);
    return ct;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <cc_path> <privkey_path> <input_encfile> <output_file>"
                  << std::endl;
        return 1;
    }

    std::string cc_path        = argv[1];
    std::string privkey_path   = argv[2];
    std::string input_encfile  = argv[3];
    std::string output_file    = argv[4];

    // Step 1: Load CryptoContext
    CryptoContext<DCRTPoly> cc;
    if (!Serial::DeserializeFromFile(cc_path, cc, SerType::JSON)) {
        std::cerr << "[decrypt] ERROR: Failed to load CryptoContext from " << cc_path << std::endl;
        return 1;
    }
    std::cout << "[decrypt] CryptoContext loaded\n";

    // Step 2: Load Private Key
    PrivateKey<DCRTPoly> privKey;
    if (!Serial::DeserializeFromFile(privkey_path, privKey, SerType::JSON)) {
        std::cerr << "[decrypt] ERROR: Failed to load private key from " << privkey_path << std::endl;
        return 1;
    }
    std::cout << "[decrypt] Private key loaded\n";

    // Step 3: Load Encrypted Weights JSON
    json encJson;
    {
        std::ifstream f(input_encfile);
        if (!f.is_open()) {
            std::cerr << "[decrypt] ERROR: Could not open input file: " << input_encfile << std::endl;
            return 1;
        }
        f >> encJson;
    }
    std::cout << "[decrypt] Encrypted weights loaded\n";

    // Step 4: Prepare plaintext JSON
    json plainJson;
    plainJson["weights_summary"] = json::array();

    for (auto& encLayer : encJson["weights_summary"]) {
        json plainLayer;
        plainLayer["layer"] = encLayer["layer"];
        plainLayer["shape"] = encLayer["shape"];

        // Mean
        {
            auto ct = decodeCiphertext(encLayer["mean"]);
            Plaintext pt;
            cc->Decrypt(privKey, ct, &pt);
            pt->SetLength(1);
            plainLayer["mean"] = pt->GetRealPackedValue()[0];
        }

        // StdDev
        {
            auto ct = decodeCiphertext(encLayer["std_dev"]);
            Plaintext pt;
            cc->Decrypt(privKey, ct, &pt);
            pt->SetLength(1);
            plainLayer["std_dev"] = pt->GetRealPackedValue()[0];
        }

       // Sample values
{
    std::vector<double> samples;

    // compute expected number of values from shape
    size_t expected_size = 1;
    for (auto dim : encLayer["shape"]) {
        expected_size *= dim.get<size_t>();
    }

    for (auto& b64 : encLayer["values"]) {
        auto ct = decodeCiphertext(b64);
        Plaintext pt;
        cc->Decrypt(privKey, ct, &pt);
        auto vals = pt->GetRealPackedValue();
        samples.insert(samples.end(), vals.begin(), vals.end());
    }

    // trim down to the real number of weights (remove padding)
    if (samples.size() > expected_size) {
        samples.resize(expected_size);
    }

    plainLayer["values"] = samples;
}
        plainJson["weights_summary"].push_back(plainLayer);
    }

    // Step 5: Save plaintext weights
    std::ofstream outFile(output_file);
    if (!outFile.is_open()) {
        std::cerr << "[decrypt] ERROR: Failed to open output file: " << output_file << std::endl;
        return 1;
    }
    outFile << std::setw(2) << plainJson << std::endl;
    outFile.close();

    std::cout << "[decrypt] Decryption completed successfully. Output: " << output_file << std::endl;
    return 0;
}

