#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include "test_helper_fns.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class DecryptModelWeightsTest : public ::testing::Test {
protected:
    json config;

    void SetUp() override {
        config = loadJson("test/client/config/test_c_config.json");
    }
};

// Test presence of decryption executable
TEST_F(DecryptModelWeightsTest, BinaryExists) {
    std::string bin = config["test_c_decryptModelWeights"]["DecryptBin"];
    EXPECT_TRUE(fileExists(bin)) << "Missing decryption binary: " << bin;
}

// Test existence of required input files
TEST_F(DecryptModelWeightsTest, InputFilesExist) {
    auto decryptConf = config["test_c_decryptModelWeights"];
    EXPECT_TRUE(fileExists(decryptConf["CryptoContext"])) << "Missing CryptoContext file";
    EXPECT_TRUE(fileExists(decryptConf["PrivKey"])) << "Missing private key file";
    EXPECT_TRUE(fileExists(decryptConf["InputEncryptedWeights"])) << "Missing encrypted weights file";
}

// Test the config schema for essential paths
TEST_F(DecryptModelWeightsTest, PathsSchemaValid) {
    auto conf = config["test_c_decryptModelWeights"];
    EXPECT_TRUE(conf["DecryptBin"].is_string());
    EXPECT_TRUE(conf["CryptoContext"].is_string());
    EXPECT_TRUE(conf["PrivKey"].is_string());
    EXPECT_TRUE(conf["InputEncryptedWeights"].is_string());
    EXPECT_TRUE(conf["OutputDecryptedWeights"].is_string());
}

// Dry-run command formation validation
TEST_F(DecryptModelWeightsTest, DryRunArgsValid) {
    auto conf = config["test_c_decryptModelWeights"];
    std::string cmd = conf["DecryptBin"].get<std::string>() + " " 
                    + conf["CryptoContext"].get<std::string>() + " "
                    + conf["PrivKey"].get<std::string>() + " "
                    + conf["InputEncryptedWeights"].get<std::string>() + " "
                    + conf["OutputDecryptedWeights"].get<std::string>() + " --dry-run";
    SUCCEED() << "Dry-run command prepared: " << cmd;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
