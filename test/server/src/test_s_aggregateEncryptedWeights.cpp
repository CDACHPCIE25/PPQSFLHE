#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include "test_helper_fns.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class AggregateEncryptedWeightsTest : public ::testing::Test {
protected:
    json config;
    json sConf;

    void SetUp() override {
        config = loadJson("test/server/config/test_s_config.json")["test_s_aggregateEncryptedWeights"];
        sConf = loadJson(config["sConfigFile"]);
    }
};

// --- Binary existence ---
TEST_F(AggregateEncryptedWeightsTest, BinaryExists) {
    std::string bin = config["aggregateEncryptedWeightsBin"];
    EXPECT_TRUE(fileExists(bin));
}

// --- Config existence ---
TEST_F(AggregateEncryptedWeightsTest, ConfigFileExists) {
    std::string path = config["sConfigFile"];
    EXPECT_TRUE(fileExists(path));
}

// --- Schema validation ---
TEST_F(AggregateEncryptedWeightsTest, SchemaHasClientWeights) {
    ASSERT_TRUE(sConf.contains("CLIENTS"));
    auto clients = sConf["CLIENTS"];

    EXPECT_TRUE(clients.contains("CLIENT_2_ENCRYPTED_WEIGHTS_PATH"));
    EXPECT_TRUE(clients.contains("OUTPUT_DOMAIN_CHANGED_PATH"));
    EXPECT_TRUE(clients.contains("AGGREGATED_ENCRYPTED_WEIGHTS_PATH"));
}

// --- Dry-run args validation ---
TEST_F(AggregateEncryptedWeightsTest, DryRunArgsValid) {
    auto clients = sConf["CLIENTS"];
    EXPECT_TRUE(clients["CLIENT_2_ENCRYPTED_WEIGHTS_PATH"].is_string());
    EXPECT_TRUE(clients["OUTPUT_DOMAIN_CHANGED_PATH"].is_string());
    EXPECT_TRUE(clients["AGGREGATED_ENCRYPTED_WEIGHTS_PATH"].is_string());
}

// Confirm input files exist and are not empty
TEST_F(AggregateEncryptedWeightsTest, InputFilesExistAndNonEmpty) {
    auto clients = sConf["CLIENTS"];

    for (const auto& key : {"CLIENT_2_ENCRYPTED_WEIGHTS_PATH", "OUTPUT_DOMAIN_CHANGED_PATH"}) {
        std::string path = clients[key];
        EXPECT_TRUE(fileExists(path)) << "Missing input file: " << path;
        EXPECT_GT(fs::file_size(path), 10) << "Input file too small: " << path;
    }
}

// Confirm output aggregation file exists and is non-empty valid JSON
TEST_F(AggregateEncryptedWeightsTest, OutputAggregatedFileValid) {
    std::string outPath = sConf["CLIENTS"]["AGGREGATED_ENCRYPTED_WEIGHTS_PATH"];
    EXPECT_TRUE(fileExists(outPath)) << "Aggregated output file missing: " << outPath;
    EXPECT_GT(fs::file_size(outPath), 10) << "Aggregated output file empty: " << outPath;

    EXPECT_NO_THROW({
        json outJson = loadJson(outPath);
        EXPECT_FALSE(outJson.empty());
        EXPECT_TRUE(outJson.contains("weights_summary")) << "Output JSON missing 'weights_summary'";
    });
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
