#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include "test_helper_fns.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// -------------------- Tests --------------------
class KeyGenTest : public ::testing::Test {
protected:
    json testConfig;
    json config_c1;
    json config_c2;

    void SetUp() override {
        // Load test_c_config.json
        json fullConfig = loadJson("test/client/config/test_c_config.json");
        ASSERT_TRUE(fullConfig.contains("test_c_keyGen"))
            << "Missing test_c_keyGen section in test_c_config.json";
        testConfig = fullConfig["test_c_keyGen"];

        // Load per-client configs
        config_c1 = loadJson(testConfig["ConfigFile_Client1"]);
        config_c2 = loadJson(testConfig["ConfigFile_Client2"]);
    }
};

// -------------------- Config tests --------------------
TEST_F(KeyGenTest, Client1ConfigExists) {
    ASSERT_TRUE(fileExists(testConfig["ConfigFile_Client1"]));
}
TEST_F(KeyGenTest, Client2ConfigExists) {
    ASSERT_TRUE(fileExists(testConfig["ConfigFile_Client2"]));
}

// --- Schema validation ---
TEST_F(KeyGenTest, Client1SchemaHasAllKeys) {
    auto client = config_c1["CLIENT"];
    EXPECT_TRUE(client.contains("CC_PATH"));
    EXPECT_TRUE(client.contains("PUBKEY_PATH"));
    EXPECT_TRUE(client.contains("PRIVKEY_PATH"));
}
TEST_F(KeyGenTest, Client2SchemaHasAllKeys) {
    auto client = config_c2["CLIENT"];
    EXPECT_TRUE(client.contains("CC_PATH"));
    EXPECT_TRUE(client.contains("PUBKEY_PATH"));
    EXPECT_TRUE(client.contains("PRIVKEY_PATH"));
}

// --- Binary existence ---
TEST_F(KeyGenTest, KeyGenBinaryExists) {
    ASSERT_TRUE(fileExists(testConfig["KeyGenBin"]));
}

// --- Dry-run KeyGen (not generating keys here, just check args ready) ---
// -------------------- Functional run --------------------
TEST_F(KeyGenTest, RunKeyGenProducesKeys) {
    auto client = config_c1["CLIENT"];
    std::string ccPath     = client["CC_PATH"];
    std::string pubkeyPath = client["PUBKEY_PATH"];
    std::string privkeyPath = client["PRIVKEY_PATH"];


    // Check files exist
    ASSERT_TRUE(fileExists(pubkeyPath)) << "pubkey not generated";
    ASSERT_TRUE(fileExists(privkeyPath)) << "privkey not generated";

    // Check non-empty
    EXPECT_GT(fs::file_size(pubkeyPath), static_cast<uintmax_t>(0));
    EXPECT_GT(fs::file_size(privkeyPath), static_cast<uintmax_t>(0));

    // Check parse as JSON
    EXPECT_NO_THROW({
        auto pubj = loadJson(pubkeyPath);
        auto privj = loadJson(privkeyPath);
        EXPECT_FALSE(pubj.empty());
        EXPECT_FALSE(privj.empty());
    });
}

// -------------------- Main --------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
