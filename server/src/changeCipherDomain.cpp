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

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <cc_path> <rekey_path> <input_encfile> <output_encfile>"
                  << std::endl;
        return 1;
    }

    std::string cc_path       = argv[1];
    std::string rekey_path    = argv[2];
    std::string input_encfile = argv[3];
    std::string output_encfile= argv[4];

    // Step 1: Load CryptoContext
    CryptoContext<DCRTPoly> cc;
    if (!Serial::DeserializeFromFile(cc_path, cc, SerType::JSON)) {
        std::cerr << "[recrypt] ERROR: Failed to load CryptoContext: " << cc_path << std::endl;
        return 1;
    }
    std::cout << "[recrypt] CryptoContext loaded\n";

    // Step 2: Load ReEncryption Key
    EvalKey<DCRTPoly> reKey;
    if (!Serial::DeserializeFromFile(rekey_path, reKey, SerType::JSON)) {
        std::cerr << "[recrypt] ERROR: Failed to load ReKey from " << rekey_path << std::endl;
        return 1;
    }
    std::cout << "[recrypt] ReKey loaded\n";

    // Step 3: Load Encrypted Weights (client1)
    std::ifstream inFile(input_encfile);
    if (!inFile.is_open()) {
        std::cerr << "[recrypt] ERROR: Could not open input encrypted weights file\n";
        return 1;
    }
    json inputJson;
    inFile >> inputJson;
    inFile.close();

    json outputJson;
    outputJson["weights_summary"] = json::array();

    // Step 4: ReEncrypt each field
    for (auto& encLayer : inputJson["weights_summary"]) {
        json reEncLayer;
        reEncLayer["layer"] = encLayer["layer"];
        reEncLayer["shape"] = encLayer["shape"];

        // Mean
        {
            std::string mean_b64 = encLayer["mean"];
            std::string mean_bin = Base64Decode(mean_b64);
            std::stringstream ss(mean_bin);
            Ciphertext<DCRTPoly> ct;
            Serial::Deserialize(ct, ss, SerType::BINARY);

            Ciphertext<DCRTPoly> ct_re = cc->ReEncrypt(ct, reKey);

            std::stringstream ss_out;
            Serial::Serialize(ct_re, ss_out, SerType::BINARY);
            reEncLayer["mean"] = Base64Encode(ss_out.str());
        }

        // StdDev
        {
            std::string std_b64 = encLayer["std_dev"];
            std::string std_bin = Base64Decode(std_b64);
            std::stringstream ss(std_bin);
            Ciphertext<DCRTPoly> ct;
            Serial::Deserialize(ct, ss, SerType::BINARY);

            Ciphertext<DCRTPoly> ct_re = cc->ReEncrypt(ct, reKey);

            std::stringstream ss_out;
            Serial::Serialize(ct_re, ss_out, SerType::BINARY);
            reEncLayer["std_dev"] = Base64Encode(ss_out.str());
        }

        // Sample Values
        {
            std::vector<std::string> reEncSamples;
            for (auto& sample_b64 : encLayer["values"]) {
                std::string bin = Base64Decode(sample_b64);
                std::stringstream ss(bin);
                Ciphertext<DCRTPoly> ct;
                Serial::Deserialize(ct, ss, SerType::BINARY);

                Ciphertext<DCRTPoly> ct_re = cc->ReEncrypt(ct, reKey);

                std::stringstream ss_out;
                Serial::Serialize(ct_re, ss_out, SerType::BINARY);
                reEncSamples.push_back(Base64Encode(ss_out.str()));
            }
            reEncLayer["values"] = reEncSamples;
        }

        outputJson["weights_summary"].push_back(reEncLayer);
    }

    // Step 5: Save output (now in client2 domain)
    std::ofstream outFile(output_encfile);
    if (!outFile.is_open()) {
        std::cerr << "[recrypt] ERROR: Failed to open output file\n";
        return 1;
    }
    outFile << std::setw(2) << outputJson << std::endl;
    outFile.close();

    std::cout << "[recrypt] Re-encryption completed successfully. Output: " 
              << output_encfile << std::endl;

    return 0;
}
