// ============ INCLUDE SECTION ============
#include <cereal/archives/json.hpp>
#include <cereal/types/polymorphic.hpp>
#include "openfhe.h"
#include "config_core.h"
#include <nlohmann/json.hpp>  // For JSON parsing
#include <fstream>
#include <iostream>

#ifndef CONFIG_PATH
#define CONFIG_PATH "server/config/config_cc.json"  // Default path if not defined
#endif

#ifndef OUTPUT_PATH
#define OUTPUT_PATH "server/storage/CC.json"
#endif

std::string outputPath = OUTPUT_PATH;


// ======= CEREAL TYPE REGISTRATIONS =======
CEREAL_REGISTER_TYPE(lbcrypto::SchemeCKKSRNS)
CEREAL_REGISTER_TYPE(lbcrypto::CryptoParametersCKKSRNS)
CEREAL_REGISTER_DYNAMIC_INIT(libOPENFHEpke)

// =============== NAMESPACES ==============
using namespace lbcrypto;
using namespace std;
using json = nlohmann::json;

// =========== CONTEXT SETUP ===============
CryptoContext<DCRTPoly> Common_ContextSetup(const std::string& configFile) {
    CCParams<CryptoContextCKKSRNS> params;

    // Load JSON file
    std::ifstream file(configFile);
    if (!file.is_open()) {
        cerr << "Failed to open " << configFile << endl;
        exit(1);
    }

    json config;
    file >> config;

    //Apply parameters from JSON
    if (config.contains("MultiplicativeDepth"))
        params.SetMultiplicativeDepth(config["MultiplicativeDepth"]);

    if (config.contains("ScalingModSize"))
        params.SetScalingModSize(config["ScalingModSize"]);

    if (config.contains("BatchSize"))
        params.SetBatchSize(config["BatchSize"]);

    if (config.contains("PREMode")) {
        std::string mode = config["PREMode"];
        if (mode == "INDCPA") {
            params.SetPREMode(INDCPA);
        } else if (mode == "FIXED_NOISE") {
            params.SetScalingTechnique(FIXEDMANUAL);  //

        } else {
            cerr << "Unknown PREMode in config: " << mode << endl;
            exit(1);
        }
    }

    auto cc = GenCryptoContext(params);

    // Enable necessary features
    cc->Enable(PKE);
    cc->Enable(LEVELEDSHE);
    cc->Enable(MULTIPARTY);
    cc->Enable(KEYSWITCH);
    cc->Enable(ADVANCEDSHE);
    cc->Enable(PRE);

    return cc;
}

// ==================== MAIN =====================
int main() {
    std::string configPath = CONFIG_PATH;  // JSON config file path
    // Generate CC from config
    auto cc = Common_ContextSetup(configPath);

    // Serialize context
    std::string outputPath = OUTPUT_PATH;  // Output file path
    if (!Serial::SerializeToFile(outputPath, cc, SerType::JSON)) {
        cerr << "Failed to serialize CryptoContext to CC.json" << endl;
        return 1;
    }

    cout << "CryptoContext Generated and saved to : "<<OUTPUT_PATH<<endl;
    return 0;
}
