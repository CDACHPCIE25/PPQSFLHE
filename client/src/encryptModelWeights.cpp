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
                  << " <cc_path> <pubkey_path> <input_weights> <output_encfile>" 
                  << std::endl;
        return 1;
    }

    std::string cc_path        = argv[1];
    std::string pubkey_path    = argv[2];
    std::string input_weights  = argv[3];
    std::string output_encfile = argv[4];

    // Step 1: Load CryptoContext
    CryptoContext<DCRTPoly> cc;
    if (!Serial::DeserializeFromFile(cc_path, cc, SerType::JSON)) {
        std::cerr << "[encrypt] ERROR: Failed to deserialize crypto context from " << cc_path << std::endl;
        return 1;
    }
    std::cout << "[encrypt] CryptoContext loaded from " << cc_path << std::endl;

    // ? Get batch size from CryptoContext
    size_t batchSize = cc->GetEncodingParams()->GetBatchSize();
    std::cout << "[encrypt] Batch size from CryptoContext = " << batchSize << std::endl;

    // Step 2: Load Public Key
    PublicKey<DCRTPoly> publicKey;
    if (!Serial::DeserializeFromFile(pubkey_path, publicKey, SerType::JSON)) {
        std::cerr << "[encrypt] ERROR: Failed to deserialize public key from " << pubkey_path << std::endl;
        return 1;
    }
    std::cout << "[encrypt] Public key loaded from " << pubkey_path << std::endl;

    // Step 3: Read input JSON (weights)
    std::ifstream inputFile(input_weights);
    if (!inputFile.is_open()) {
        std::cerr << "[encrypt] ERROR: Could not open input weights file: " << input_weights << std::endl;
        return 1;
    }

    json inputJson;
    inputFile >> inputJson;
    inputFile.close();
    std::cout << "[encrypt] Weights loaded from " << input_weights << std::endl;

    // Step 4: Prepare output JSON structure
    json outputJson;
    outputJson["weights_summary"] = json::array();

    for (const auto& weight : inputJson["weights_summary"]) {
        std::string layerName = weight["layer"];

        // ? Skip optimizer layers
        if (layerName.rfind("optimizer/", 0) == 0) {
            std::cout << "[encrypt] Skipping optimizer layer: " << layerName << std::endl;
            continue;
        }

        json encryptedWeight;
        encryptedWeight["layer"] = layerName;
        encryptedWeight["shape"] = weight["shape"];

        // Encrypt mean
        double mean = weight["mean"];
        auto pt_mean = cc->MakeCKKSPackedPlaintext(std::vector<double>{mean});
        auto ct_mean = cc->Encrypt(publicKey, pt_mean);
        std::stringstream ss_mean;
        Serial::Serialize(ct_mean, ss_mean, SerType::BINARY);
        encryptedWeight["mean"] = Base64Encode(ss_mean.str());

        // Encrypt std_dev
        double stddev = weight["std_dev"];
        auto pt_stddev = cc->MakeCKKSPackedPlaintext(std::vector<double>{stddev});
        auto ct_stddev = cc->Encrypt(publicKey, pt_stddev);
        std::stringstream ss_stddev;
        Serial::Serialize(ct_stddev, ss_stddev, SerType::BINARY);
        encryptedWeight["std_dev"] = Base64Encode(ss_stddev.str());

        // Encrypt values in chunks of batchSize with padding
        std::vector<double> samples = weight["values"];
        std::vector<std::string> encryptedSampleBatches;

        for (size_t i = 0; i < samples.size(); i += batchSize) {
            size_t end = std::min(i + batchSize, samples.size());
            std::vector<double> batch(samples.begin() + i, samples.begin() + end);

            // Pad with zeros if smaller than batchSize
            if (batch.size() < batchSize) {
                batch.resize(batchSize, 0.0);
            }

            Plaintext pt_batch = cc->MakeCKKSPackedPlaintext(batch);
            Ciphertext<DCRTPoly> ct_batch = cc->Encrypt(publicKey, pt_batch);

            std::stringstream ss_batch;
            Serial::Serialize(ct_batch, ss_batch, SerType::BINARY);
            encryptedSampleBatches.push_back(Base64Encode(ss_batch.str()));
        }

        encryptedWeight["values"] = encryptedSampleBatches;

        outputJson["weights_summary"].push_back(encryptedWeight);
    }

    // Step 5: Write encrypted data
    std::ofstream outputFile(output_encfile);
    if (!outputFile.is_open()) {
        std::cerr << "[encrypt] ERROR: Failed to write to output file: " << output_encfile << std::endl;
        return 1;
    }

    outputFile << std::setw(2) << outputJson << std::endl;
    outputFile.close();

    std::cout << "[encrypt] Encryption completed successfully and saved in " << output_encfile << std::endl;
    return 0;
}

