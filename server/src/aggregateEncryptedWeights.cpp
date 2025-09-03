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

Ciphertext<DCRTPoly> decodeCiphertext(CryptoContext<DCRTPoly> cc, const std::string& b64) {
    std::string bin = Base64Decode(b64);
    std::stringstream ss(bin);
    Ciphertext<DCRTPoly> ct;
    Serial::Deserialize(ct, ss, SerType::BINARY);
    return ct;
}

std::string encodeCiphertext(Ciphertext<DCRTPoly> ct) {
    std::stringstream ss;
    Serial::Serialize(ct, ss, SerType::BINARY);
    return Base64Encode(ss.str());
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <cc_path> <client2_encfile> <client1to2_encfile> <output_aggfile>"
                  << std::endl;
        return 1;
    }

    std::string cc_path         = argv[1];
    std::string client2_file    = argv[2];
    std::string client1to2_file = argv[3];
    std::string output_file     = argv[4];

    // Step 1: Load CryptoContext
    CryptoContext<DCRTPoly> cc;
    if (!Serial::DeserializeFromFile(cc_path, cc, SerType::JSON)) {
        std::cerr << "[agg] ERROR: Failed to load CryptoContext from " << cc_path << std::endl;
        return 1;
    }
    std::cout << "[agg] CryptoContext loaded\n";

    // Step 2: Load input JSONs
    json c2_json, c1to2_json;
    {
        std::ifstream f(client2_file);
        f >> c2_json;
    }
    {
        std::ifstream f(client1to2_file);
        f >> c1to2_json;
    }

    json outputJson;
    outputJson["weights_summary"] = json::array();

    // Step 3: Aggregate only if same layer + same shape
    for (const auto& w2 : c2_json["weights_summary"]) {
        for (const auto& w1 : c1to2_json["weights_summary"]) {

            if (w1["layer"] != w2["layer"]) continue;
            if (w1["shape"] != w2["shape"]) continue;

            json aggLayer;
            aggLayer["layer"] = w2["layer"];
            aggLayer["shape"] = w2["shape"];

            // Mean
            {
                auto ct1 = decodeCiphertext(cc, w1["mean"]);
                auto ct2 = decodeCiphertext(cc, w2["mean"]);
                auto ct_sum = cc->EvalAdd(ct1, ct2);
                auto ct_avg = cc->EvalMult(ct_sum, 0.5);
                aggLayer["mean"] = encodeCiphertext(ct_avg);
            }

            // StdDev
            {
                auto ct1 = decodeCiphertext(cc, w1["std_dev"]);
                auto ct2 = decodeCiphertext(cc, w2["std_dev"]);
                auto ct_sum = cc->EvalAdd(ct1, ct2);
                auto ct_avg = cc->EvalMult(ct_sum, 0.5);
                aggLayer["std_dev"] = encodeCiphertext(ct_avg);
            }

            // Values
            {
                std::vector<std::string> aggValues;
                auto v1 = w1["values"];
                auto v2 = w2["values"];
                size_t n = std::min(v1.size(), v2.size());

                for (size_t j = 0; j < n; j++) {
                    auto ct1 = decodeCiphertext(cc, v1[j]);
                    auto ct2 = decodeCiphertext(cc, v2[j]);
                    auto ct_sum = cc->EvalAdd(ct1, ct2);
                    auto ct_avg = cc->EvalMult(ct_sum, 0.5);
                    aggValues.push_back(encodeCiphertext(ct_avg));
                }
                aggLayer["values"] = aggValues;
            }

            outputJson["weights_summary"].push_back(aggLayer);
        }
    }

    // Step 4: Save aggregated result
    std::ofstream outFile(output_file);
    outFile << std::setw(2) << outputJson << std::endl;
    outFile.close();

    std::cout << "[agg] Aggregation completed successfully. Output: " << output_file << std::endl;
    return 0;
}

