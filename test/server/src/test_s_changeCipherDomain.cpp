#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <filesystem>

#include "test_helper_fns.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class ChangeCipherDomainTest : public ::testing::Test {
protected:
    json config;
    json sConf;

    void SetUp() override {
        config = loadJson("test/server/config/test_s_config.json")["test_s_changeCipherDomain"];
        sConf  = loadJson(config["sConfigFile"]);
    }
};

// --- Binary existence ---
TEST_F(ChangeCipherDomainTest, BinaryExists) {
    std::string bin = config["changeCipherDomainBin"];
    EXPECT_TRUE(fileExists(bin));
}

// --- Config existence ---
TEST_F(ChangeCipherDomainTest, ConfigFileExists) {
    std::string path = config["sConfigFile"];
    EXPECT_TRUE(fileExists(path));
}

// --- Schema validation ---
TEST_F(ChangeCipherDomainTest, SchemaHasClientKeys) {

    ASSERT_TRUE(sConf.contains("CLIENTS"));
    auto clients = sConf["CLIENTS"];

    EXPECT_TRUE(clients.contains("CLIENT_1_REKEY"));
    EXPECT_TRUE(clients.contains("CLIENT_2_REKEY"));
    EXPECT_TRUE(clients.contains("CLIENT_1_ENCRYPTED_WEIGHTS_PATH"));
    EXPECT_TRUE(clients.contains("OUTPUT_DOMAIN_CHANGED_PATH"));
    EXPECT_TRUE(clients.contains("AGGREGATED_ENCRYPTED_WEIGHTS_PATH"));
    EXPECT_TRUE(clients.contains("OUTPUT_AGGREGATED_DOMAIN_CHANGED_PATH"));
    
}

// --- Dry-run args validation ---
TEST_F(ChangeCipherDomainTest, DryRunArgsValid) {
    auto clients = sConf["CLIENTS"];
    EXPECT_TRUE(clients["CLIENT_1_REKEY"].is_string());
    EXPECT_TRUE(clients["CLIENT_2_REKEY"].is_string());
    EXPECT_TRUE(clients["CLIENT_1_ENCRYPTED_WEIGHTS_PATH"].is_string());
    EXPECT_TRUE(clients["OUTPUT_DOMAIN_CHANGED_PATH"].is_string());
}

// --- Output file validation ---
TEST_F(ChangeCipherDomainTest, DomainChangedFileValid) {
    auto out = sConf["CLIENTS"]["OUTPUT_DOMAIN_CHANGED_PATH"];
    ASSERT_TRUE(fileExists(out)) << "Output domain-changed file missing: " << out;
    ASSERT_GT(fs::file_size(out), 0) << "Domain-changed file empty.";

    EXPECT_NO_THROW({
        auto outJson = loadJson(out);
        EXPECT_FALSE(outJson.empty());
    });
}
