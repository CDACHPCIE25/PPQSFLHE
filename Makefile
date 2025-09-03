# ============================
# Makefile - PPFL
# ============================

#INCLUDE_OPENFHE

#INCLUDE_UTILS

#INCLUDE := INCLUDE_OPENFHE INCLUDE_UTILS

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
            -I./lib/openfhe-development/src/pke/include \
            -I./lib/openfhe-development/src/core/include \
            -I./lib/openfhe-development/src/binfhe/include \
            -I./lib/openfhe-development/build/src/core \
            -I./lib/openfhe-development/third-party/cereal/include \
            -I./utils/json/include \
            -I./lib \
            -I./test \
            -I./lib/openfhe-development/third-party/google-test/googletest/include \
            -DCEREAL_RAPIDJSON_HAS_CXX11_NOEXCEPT=1 \
            -DCONFIG_PATH=\"$(abspath server/config/config_cc.json)\" \
            -DOUTPUT_PATH=\"$(abspath server/storage/CC.json)\"

# for every module, we need to take care
            
CXXFLAGS += -Wno-unknown-pragmas
CXXFLAGS += -Wno-missing-field-initializers

LDFLAGS  := -L./lib/openfhe-development/build/lib \
            -Wl,--whole-archive -lOPENFHEpke -lOPENFHEcore -lOPENFHEbinfhe -Wl,--no-whole-archive \
            -lssl -lcrypto -lpthread -ldl -lm

# ----- gtest linking flags -----  
TEST_LDFLAGS := -lgtest -lgtest_main -pthread

# ----- Project Directories -----
SERVER_SRC_DIR   := server/src
SERVER_BUILD_DIR := server/build
CLIENT_SRC_DIR   := client/src
CLIENT_BUILD_DIR := client/build

# ----- Test Directories ------
TEST_SERVER_SRC_DIR   := test/server/src
TEST_SERVER_BUILD_DIR := test/server/build
TEST_CLIENT_SRC_DIR   := test/client/src
TEST_CLIENT_BUILD_DIR := test/client/build

#=============Project Build===============

# ----- genCC -----
GENCC_SRC := $(SERVER_SRC_DIR)/genCC.cpp
GENCC_BIN := $(SERVER_BUILD_DIR)/genCC

# ----- runMserver -----
MONGOOSE_SRC := lib/mongoose/mongoose.c
RUNMSERVER_SRC := $(SERVER_SRC_DIR)/runMserver.cpp
RUNMSERVER_BIN := $(SERVER_BUILD_DIR)/runMserver

# ----- client keyGen -----
KEYGEN_SRC := $(CLIENT_SRC_DIR)/keyGen.cpp
KEYGEN_BIN := $(CLIENT_BUILD_DIR)/keyGen

#----- client REkeyGEn-------
REKEYGEN_SRC := $(CLIENT_SRC_DIR)/REkeyGen.cpp
REKEYGEN_BIN := $(CLIENT_BUILD_DIR)/REkeyGen

# ----- client encryptModelWeights -----
ENCRYPTMODELWEIGTHS_SRC := $(CLIENT_SRC_DIR)/encryptModelWeights.cpp
ENCRYPTMODELWEIGTHS_BIN := $(CLIENT_BUILD_DIR)/encryptModelWeights

#----- changeCipherDomain ---
CHANGECIPHERDOMAIN_SRC := $(SERVER_SRC_DIR)/changeCipherDomain.cpp
CHANGECIPHERDOMAIN_BIN := $(SERVER_BUILD_DIR)/changeCipherDomain

#----- aggrRun ---
AGGREGATEENCRYPTEDWEIGHTS_SRC := $(SERVER_SRC_DIR)/aggregateEncryptedWeights.cpp
AGGREGATEENCRYPTEDWEIGHTS_BIN := $(SERVER_BUILD_DIR)/aggregateEncryptedWeights

# ----- client decryptModelWeights -----
DECRYPTMODELWEIGTHS_SRC := $(CLIENT_SRC_DIR)/decryptModelWeights.cpp
DECRYPTMODELWEIGTHS_BIN := $(CLIENT_BUILD_DIR)/decryptModelWeights

# ==============================
# Default project targets
all: $(GENCC_BIN) $(RUNMSERVER_BIN) $(KEYGEN_BIN) $(REKEYGEN_BIN) $(ENCRYPTMODELWEIGTHS_BIN) $(CHANGECIPHERDOMAIN_BIN) $(AGGREGATEENCRYPTEDWEIGHTS_BIN) $(DECRYPTMODELWEIGTHS_BIN)

# ===== genCC build =====
genCC: $(GENCC_BIN)
$(GENCC_BIN): $(GENCC_SRC)
	@mkdir -p $(SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# ===== runMserver build =====
runMserver: $(RUNMSERVER_BIN)
$(RUNMSERVER_BIN): $(RUNMSERVER_SRC) $(MONGOOSE_SRC)
	@mkdir -p $(SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ -I./lib/mongoose $(LDFLAGS)
 
# ====== keyGen build =======
keyGen: $(KEYGEN_BIN)
$(KEYGEN_BIN): $(KEYGEN_SRC)
	@mkdir -p $(CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
 
# ----- client reKeyGen build -----
REkeyGen: $(REKEYGEN_BIN)
$(REKEYGEN_BIN): $(REKEYGEN_SRC)
	@mkdir -p $(CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# ====== encryptModelWeights build =======
encryptModelWeights: $(ENCRYPTMODELWEIGTHS_BIN)
$(ENCRYPTMODELWEIGTHS_BIN): $(ENCRYPTMODELWEIGTHS_SRC)
	@mkdir -p $(CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

#----- changeCipherDomain build ---
changeCipherDomain: $(CHANGECIPHERDOMAIN_BIN)
$(CHANGECIPHERDOMAIN_BIN): $(CHANGECIPHERDOMAIN_SRC)
	@mkdir -p $(SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
 
#----- aggrRun build ---
aggregateEncryptedWeights: $(AGGREGATEENCRYPTEDWEIGHTS_BIN)
$(AGGREGATEENCRYPTEDWEIGHTS_BIN): $(AGGREGATEENCRYPTEDWEIGHTS_SRC)
	@mkdir -p $(SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
 
 
# ----- decryptModelWeights build ------
decryptModelWeights: $(DECRYPTMODELWEIGTHS_BIN)
$(DECRYPTMODELWEIGTHS_BIN): $(DECRYPTMODELWEIGTHS_SRC)
	@mkdir -p $(CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# ===== clean =====
clean:
	rm -rf $(SERVER_BUILD_DIR) $(CLIENT_BUILD_DIR)
 
# ============================
# ----- Test targets ------

# ----- Test Files -----
TEST_S_CC_SRC := $(TEST_SERVER_SRC_DIR)/test_s_CC.cpp
TEST_S_CC_BIN := $(TEST_SERVER_BUILD_DIR)/test_s_CC

# ----- runMserver -----
TEST_S_RUNMSERVER_SRC := $(TEST_SERVER_SRC_DIR)/test_s_runMserver.cpp
TEST_S_RUNMSERVER_BIN := $(TEST_SERVER_BUILD_DIR)/test_s_runMserver

# ----- Client Tests -----
TEST_C_KEYGEN_SRC := $(TEST_CLIENT_SRC_DIR)/test_c_keyGen.cpp
TEST_C_KEYGEN_BIN := $(TEST_CLIENT_BUILD_DIR)/test_c_keyGen

TEST_C_REKEYGEN_SRC := $(TEST_CLIENT_SRC_DIR)/test_c_REkeyGen.cpp
TEST_C_REKEYGEN_BIN := $(TEST_CLIENT_BUILD_DIR)/test_c_REkeyGen

# ----- Client encryptModelWeights Test -----
TEST_C_ENCRYPT_SRC := $(TEST_CLIENT_SRC_DIR)/test_c_encryptModelWeights.cpp
TEST_C_ENCRYPT_BIN := $(TEST_CLIENT_BUILD_DIR)/test_c_encryptModelWeights

# ----- changeCipherDomain Test -----
TEST_S_CHANGECIPHERDOMAIN_SRC := $(TEST_SERVER_SRC_DIR)/test_s_changeCipherDomain.cpp
TEST_S_CHANGECIPHERDOMAIN_BIN := $(TEST_SERVER_BUILD_DIR)/test_s_changeCipherDomain

# ----- aggregateEncryptedWeights Test -----
TEST_S_AGGREGATE_SRC := $(TEST_SERVER_SRC_DIR)/test_s_aggregateEncryptedWeights.cpp
TEST_S_AGGREGATE_BIN := $(TEST_SERVER_BUILD_DIR)/test_s_aggregateEncryptedWeights

# ----- client decryptModelWeights Test -----
TEST_C_DECRYPT_SRC := $(TEST_CLIENT_SRC_DIR)/test_c_decryptModelWeights.cpp
TEST_C_DECRYPT_BIN := $(TEST_CLIENT_BUILD_DIR)/test_c_decryptModelWeights

#======= Testing Builds ===================

# ----- Build test_s_CC -----
test_s_CC: $(TEST_S_CC_BIN)
$(TEST_S_CC_BIN): $(TEST_S_CC_SRC)
	@mkdir -p $(TEST_SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)
 
# ----- Build test_s_runMserver -----
test_s_runMserver: $(TEST_S_RUNMSERVER_BIN)
$(TEST_S_RUNMSERVER_BIN): $(TEST_S_RUNMSERVER_SRC)
	@mkdir -p $(TEST_SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)

# ----- Build test_c_keyGen -----
test_c_keyGen: $(TEST_C_KEYGEN_BIN)
$(TEST_C_KEYGEN_BIN): $(TEST_C_KEYGEN_SRC)
	@mkdir -p $(TEST_CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)
 
# ----- Build test_c_REkeyGen -----
test_c_REkeyGen: $(TEST_C_REKEYGEN_BIN)
$(TEST_C_REKEYGEN_BIN): $(TEST_C_REKEYGEN_SRC)
	@mkdir -p $(TEST_CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)

# ----- Build test_c_encryptModelWeights -----
test_c_encryptModelWeights: $(TEST_C_ENCRYPT_BIN)
$(TEST_C_ENCRYPT_BIN): $(TEST_C_ENCRYPT_SRC)
	@mkdir -p $(TEST_CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)
 

# ----- Build test_s_changeCipherDomain -----
test_s_changeCipherDomain: $(TEST_S_CHANGECIPHERDOMAIN_BIN)
$(TEST_S_CHANGECIPHERDOMAIN_BIN): $(TEST_S_CHANGECIPHERDOMAIN_SRC)
	@mkdir -p $(TEST_SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)
 
# ----- Build test_s_aggregateEncryptedWeights -----
test_s_aggregateEncryptedWeights: $(TEST_S_AGGREGATE_BIN)
$(TEST_S_AGGREGATE_BIN): $(TEST_S_AGGREGATE_SRC)
	@mkdir -p $(TEST_SERVER_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)
 
# ----- Build test_c_decryptModelWeights -----
test_c_decryptModelWeights: $(TEST_C_DECRYPT_BIN)
$(TEST_C_DECRYPT_BIN): $(TEST_C_DECRYPT_SRC)
	@mkdir -p $(TEST_CLIENT_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(TEST_LDFLAGS)

# Build all tests
test_all: test_s_CC test_s_runMserver test_c_keyGen test_c_REkeyGen test_c_encryptModelWeights test_s_changeCipherDomain test_s_aggregateEncryptedWeights test_c_decryptModelWeights

#.PHONY: all clean
.PHONY: all clean \
        genCC runMserver keyGen REkeyGen encryptModelWeights \
        changeCipherDomain aggregateEncryptedWeights decryptModelWeights \
        test test_all test_s_CC test_s_runMserver test_c_keyGen test_c_REkeyGen test_c_encryptModelWeights test_s_changeCipherDomain test_s_aggregateEncryptedWeights test_c_decryptModelWeights
 