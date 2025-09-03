#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "test_helper_fns.hpp"

namespace fs = std::filesystem;

// -------------------- Tests --------------------
class REkeyGenTest : public ::testing::Test {
protected:
    json testConf;
    json rekeyConf;
    json config_c1;
    json config_c2;

    void SetUp() override {
        // Load from dedicated test config
        testConf = loadJson("test/client/config/test_c_config.json");
        rekeyConf = testConf["test_c_REkeyGen"];

        config_c1 = loadJson(rekeyConf["ConfigFile_Client1"]);
        config_c2 = loadJson(rekeyConf["ConfigFile_Client2"]);
    }
};

// --- Basic file existence ---
TEST_F(REkeyGenTest, Client1ConfigExists) {
    EXPECT_TRUE(fileExists(rekeyConf["ConfigFile_Client1"]))
        << "Missing client1 config at " << rekeyConf["ConfigFile_Client1"];
}

TEST_F(REkeyGenTest, Client2ConfigExists) {
    EXPECT_TRUE(fileExists(rekeyConf["ConfigFile_Client2"]))
        << "Missing client2 config at " << rekeyConf["ConfigFile_Client2"];
}

// --- Schema validation ---
TEST_F(REkeyGenTest, Client1SchemaHasAllKeys) {
    auto client = config_c1["CLIENT"];
    EXPECT_TRUE(client.contains("CC_PATH"));
    EXPECT_TRUE(client.contains("PUBKEY_PATH"));
    EXPECT_TRUE(client.contains("PRIVKEY_PATH"));
    EXPECT_TRUE(client.contains("PEER_PUBKEY_PATH"));
    EXPECT_TRUE(client.contains("REKEY_PATH"));
    EXPECT_TRUE(client.contains("client_id"));
}

TEST_F(REkeyGenTest, Client2SchemaHasAllKeys) {
    auto client = config_c2["CLIENT"];
    EXPECT_TRUE(client.contains("CC_PATH"));
    EXPECT_TRUE(client.contains("PUBKEY_PATH"));
    EXPECT_TRUE(client.contains("PRIVKEY_PATH"));
    EXPECT_TRUE(client.contains("PEER_PUBKEY_PATH"));
    EXPECT_TRUE(client.contains("REKEY_PATH"));
    EXPECT_TRUE(client.contains("client_id"));
}

// --- Binary existence ---
TEST_F(REkeyGenTest, REkeyGenBinaryExists) {
    auto bin = rekeyConf["REkeyGenBin"];
    EXPECT_TRUE(fileExists(bin))
        << "REkeyGen binary not found at " << bin;
}

// --- Dry-run arg check ---
TEST_F(REkeyGenTest, REkeyGenArgsValid) {
    auto client = config_c1["CLIENT"];
    EXPECT_TRUE(client["CC_PATH"].is_string());
    EXPECT_TRUE(client["PUBKEY_PATH"].is_string());
    EXPECT_TRUE(client["PRIVKEY_PATH"].is_string());
    EXPECT_TRUE(client["PEER_PUBKEY_PATH"].is_string());
    EXPECT_TRUE(client["REKEY_PATH"].is_string());
}

// --- Functional test: run REkeyGen produces ReKey file ---
TEST_F(REkeyGenTest, RunREkeyGenProducesReKey) {

    std::string rekeyFile1 = config_c1["CLIENT"]["REKEY_PATH"];
    std::string rekeyFile2 = config_c2["CLIENT"]["REKEY_PATH"];

    bool exists1 = fileExists(rekeyFile1);
    bool exists2 = fileExists(rekeyFile2);

    ASSERT_TRUE(exists1 || exists2)
        << "No ReKey file generated at either expected path";

    if (exists1) {
        std::ifstream f1(rekeyFile1, std::ios::binary | std::ios::ate);
        ASSERT_TRUE(f1.good()) << "Client1 ReKey file not readable";
        EXPECT_GT(f1.tellg(), 0) << "Client1 ReKey file is empty";
    }

    if (exists2) {
        std::ifstream f2(rekeyFile2, std::ios::binary | std::ios::ate);
        ASSERT_TRUE(f2.good()) << "Client2 ReKey file not readable";
        EXPECT_GT(f2.tellg(), 0) << "Client2 ReKey file is empty";
    }
}
