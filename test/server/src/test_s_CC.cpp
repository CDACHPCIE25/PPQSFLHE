#include <gtest/gtest.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "test_helper_fns.hpp"

using json = nlohmann::json;

// ---------- Fixture ----------
class ServerConfigTest : public ::testing::Test {
protected:
    json testConfig;
    json runtimeConfig;

    void SetUp() override {
        // Load whole test_s_config.json
        json fullConfig = loadJson("test/server/config/test_s_config.json");

        // Pick only this module's config
        ASSERT_TRUE(fullConfig.contains("test_s_CC"))
            << "Missing test_s_CC section in test_s_config.json";
        testConfig = fullConfig["test_s_CC"];

        // Load actual runtime config_cc.json
        std::string configFile = testConfig["ConfigFile"];
        runtimeConfig = loadJson(configFile);
    }
};

// ---------- Config Validation Tests ----------
TEST_F(ServerConfigTest, ConfigFileExists) {
    std::string configFile = testConfig["ConfigFile"];
    ASSERT_TRUE(fileExists(configFile))
        << "Config file does not exist at " << configFile;
}

TEST_F(ServerConfigTest, SchemaHasAllKeys) {
    EXPECT_TRUE(runtimeConfig.contains("MultiplicativeDepth"));
    EXPECT_TRUE(runtimeConfig.contains("ScalingModSize"));
    EXPECT_TRUE(runtimeConfig.contains("BatchSize"));
    EXPECT_TRUE(runtimeConfig.contains("PREMode"));
}

TEST_F(ServerConfigTest, MultiplicativeDepthValid) {
    int depth = runtimeConfig["MultiplicativeDepth"];
    EXPECT_GE(depth, 1) << "MultiplicativeDepth must be >= 1, got: " << depth;
    EXPECT_LE(depth, 20) << "MultiplicativeDepth too large (runtime blow up), got: " << depth;
}

TEST_F(ServerConfigTest, ScalingModSizeValid) {
    int size = runtimeConfig["ScalingModSize"];
    EXPECT_GT(size, 30) << "ScalingModSize too small (<30), got: " << size;
    EXPECT_LT(size, 100) << "ScalingModSize too large (>100), got: " << size;
}

TEST_F(ServerConfigTest, BatchSizeValid) {
    int bsz = runtimeConfig["BatchSize"];
    EXPECT_GT(bsz, 0) << "BatchSize must be > 0, got: " << bsz;
    EXPECT_LE(bsz, 8192) << "BatchSize too large (inefficient), got: " << bsz;
}

TEST_F(ServerConfigTest, PREModeValid) {
    std::string mode = runtimeConfig["PREMode"];
    EXPECT_TRUE(mode == "INDCPA" || mode == "INDCCA")
        << "PREMode must be INDCPA or INDCCA, got: " << mode;
}

// ---------- File Existence Tests ----------
TEST_F(ServerConfigTest, BinaryExists) {
    std::string genCCBin = testConfig["GenCCBin"];
    ASSERT_TRUE(fileExists(genCCBin))
        << "genCC binary does not exist at " << genCCBin;
}

TEST_F(ServerConfigTest, CCFileExists) {
    std::string ccFile = testConfig["CCFile"];
    ASSERT_TRUE(fileExists(ccFile))
        << "CC.json does not exist at " << ccFile;
}

TEST_F(ServerConfigTest, CCFileNotEmpty) {
    std::string ccFile = testConfig["CCFile"];
    std::ifstream file(ccFile, std::ios::binary | std::ios::ate);

    ASSERT_TRUE(file.good())
        << "CC.json should be readable";

    EXPECT_GT(file.tellg(), 0)
        << "CC.json should not be empty";
}

// ---------- Main ----------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
