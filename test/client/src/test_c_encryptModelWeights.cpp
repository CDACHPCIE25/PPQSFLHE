#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include "test_helper_fns.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// -------------------- Test Fixture --------------------
class EncryptModelWeightsTest : public ::testing::Test {
protected:
    json config;

    void SetUp() override {
        config = loadJson("test/client/config/test_c_config.json")["test_c_encryptModelWeights"];
    }
};

// -------------------- Tests --------------------

// --- Binary existence ---
TEST_F(EncryptModelWeightsTest, BinaryExists) {
    EXPECT_TRUE(fileExists(config["EncryptBin"]))
        << "encryptModelWeights binary missing: " << config["EncryptBin"];
}

// --- Input config sanity ---
TEST_F(EncryptModelWeightsTest, InputFilesExist) {
    EXPECT_TRUE(fileExists(config["CryptoContext"]))
        << "Missing CryptoContext: " << config["CryptoContext"];
    EXPECT_TRUE(fileExists(config["PubKey"]))
        << "Missing PublicKey: " << config["PubKey"];
    EXPECT_TRUE(fileExists(config["InputWeights"]))
        << "Missing input weights JSON: " << config["InputWeights"];
}

// --- Input JSON schema validation ---
TEST_F(EncryptModelWeightsTest, InputWeightsSchemaValid) {
    auto inputJson = loadJson(config["InputWeights"]);
    ASSERT_TRUE(inputJson.contains("weights_summary"));
    ASSERT_TRUE(inputJson["weights_summary"].is_array());

    for (auto& w : inputJson["weights_summary"]) {
        EXPECT_TRUE(w.contains("layer"));
        EXPECT_TRUE(w.contains("shape"));
        EXPECT_TRUE(w.contains("mean"));
        EXPECT_TRUE(w.contains("std_dev"));
        EXPECT_TRUE(w.contains("values"));
    }
}

// --- Output file validation (no run, just check) ---
TEST_F(EncryptModelWeightsTest, OutputEncryptedFileValid) {
    std::string out = config["OUTPUT_ENCRYPTED_WEIGHTS_PATH"];

    // Verify output file existence
    ASSERT_TRUE(fileExists(out)) << "Encrypted weights file not created: " << out;

    // Verify non-empty
    ASSERT_GT(fs::file_size(out), 0) << "Encrypted weights file is empty.";

    // Verify valid JSON
    EXPECT_NO_THROW({
        auto encJson = loadJson(out);
        EXPECT_FALSE(encJson.empty());
    });
}
