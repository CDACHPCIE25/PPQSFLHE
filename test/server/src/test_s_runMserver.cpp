#include <gtest/gtest.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "test_helper_fns.hpp"

using json = nlohmann::json;

/**
 * @brief Unit tests for running the Mserver logic.
 * 
 * These tests validate presence of sConfig.json, required keys,
 * and sanity checks on parameters.
 */
class RunMserverTest : public ::testing::Test {
protected:
    json testConfig;
    json runtimeConfig;

    void SetUp() override {
        // Load whole test_s_config.json
        json fullConfig = loadJson("test/server/config/test_s_config.json");

        // Extract only runMserver test section
        ASSERT_TRUE(fullConfig.contains("test_s_runMserver"))
            << "Missing test_s_runMserver section in test_s_config.json";
        testConfig = fullConfig["test_s_runMserver"];

        // Load sConfig.json
        ASSERT_TRUE(testConfig.contains("sConfigFile"))
            << "sConfigFile missing in test_s_runMserver section";
        std::string sConfigPath = testConfig["sConfigFile"];
        runtimeConfig = loadJson(sConfigPath);
    }
};

// ---------- File Existence ----------
TEST_F(RunMserverTest, sConfigFileExists) {
    std::string sConfigPath = testConfig["sConfigFile"];
    ASSERT_TRUE(fileExists(sConfigPath))
        << "sConfig.json does not exist at " << sConfigPath;
}

TEST_F(RunMserverTest, RunMserverBinaryExists) {
    std::string binPath = testConfig["runMserverBin"];
    ASSERT_TRUE(fileExists(binPath))
        << "RunMserver binary does not exist at " << binPath;
}

// ---------- Schema Validation ----------
TEST_F(RunMserverTest, SchemaHasAllKeys) {
    EXPECT_TRUE(runtimeConfig.contains("mSConfig"));
    EXPECT_TRUE(runtimeConfig.contains("CC"));
    EXPECT_TRUE(runtimeConfig.contains("CLIENTS"));
}

// ---------- Parameter Validation ----------
TEST_F(RunMserverTest, ServerIPValid) {
    std::string ip = runtimeConfig["mSConfig"]["SERVER_IP"];
    EXPECT_FALSE(ip.empty()) << "SERVER_IP must not be empty";
    // Could later add regex validation for IPv4/IPv6
}

TEST_F(RunMserverTest, ServerPortValid) {
    int port = runtimeConfig["mSConfig"]["SERVER_PORT"];
    EXPECT_GE(port, 1024) << "SERVER_PORT should be >= 1024, got: " << port;
    EXPECT_LE(port, 65535) << "SERVER_PORT should be <= 65535, got: " << port;
}

TEST_F(RunMserverTest, CCPathExists) {
    std::string ccFile = runtimeConfig["CC"]["path"];
    EXPECT_TRUE(fileExists(ccFile))
        << "CC.json path missing or invalid: " << ccFile;
}

TEST_F(RunMserverTest, ClientsSectionHasKeys) {
    auto clients = runtimeConfig["CLIENTS"];
    EXPECT_TRUE(clients.contains("CLIENT_1_REKEY"));
    EXPECT_TRUE(clients.contains("CLIENT_2_REKEY"));
    EXPECT_TRUE(clients.contains("CLIENT_1_ENCRYPTED_WEIGHTS_PATH"));
    EXPECT_TRUE(clients.contains("CLIENT_2_ENCRYPTED_WEIGHTS_PATH"));
    EXPECT_TRUE(clients.contains("OUTPUT_DOMAIN_CHANGED_PATH"));
    EXPECT_TRUE(clients.contains("AGGREGATED_ENCRYPTED_WEIGHTS_PATH"));
    EXPECT_TRUE(clients.contains("OUTPUT_AGGREGATED_DOMAIN_CHANGED_PATH"));
}

// ---------- Placeholder for functional integration ----------
TEST_F(RunMserverTest, RunMserverPlaceholder) {
    SUCCEED() << "TODO: hook into runMserver() and assert serving endpoints.";
}

// ---------- Main ----------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
