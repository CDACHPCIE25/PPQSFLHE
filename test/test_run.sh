#!/bin/bash
# =====================================
# PPFL Test Runner
# =====================================

set -e  # Exit immediately on failure

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$SCRIPT_DIR/.."

cd "$BASE_DIR"

# --- Run test_s_CC ---
echo "[TEST] Running test_s_CC (using test/server/config/test_s_config.json)..."
./test/server/build/test_s_CC --config test/server/config/test_s_config.json

# --- Run test_s_runMserver ---
echo "[TEST] Running test_s_runMserver (using test/server/config/test_s_config.json)..."
./test/server/build/test_s_runMserver --config test/server/config/test_s_config.json

# --- Run test_c_keyGen ---
echo "[TEST] Running test_c_keyGen (using client/config/client_1/c_config.json & client/config/client_2/c_config.json)..."
./test/client/build/test_c_keyGen

# --- Run test_c_REkeyGen ---
echo "[TEST] Running test_c_REkeyGen (using test/client/config/test_c_config.json)..."
./test/client/build/test_c_REkeyGen --config test/client/config/test_c_config.json

# --- Run test_c_encryptModelWeights ---
echo "[TEST] Running test_c_encryptModelWeights (using test/client/config/test_c_config.json)..."
./test/client/build/test_c_encryptModelWeights --config test/client/config/test_c_config.json

# --- Run test_s_changeCipherDomain ---
echo "[TEST] Running test_s_changeCipherDomain (using test/server/config/test_s_config.json)..."
./test/server/build/test_s_changeCipherDomain --config test/server/config/test_s_config.json

# --- Run test_s_aggregateEncryptedWeights ---
echo "[TEST] Running test_s_aggregateEncryptedWeights (using test/server/config/test_s_config.json)..."
./test/server/build/test_s_aggregateEncryptedWeights --config test/server/config/test_s_config.json

# --- Run test_c_decryptModelWeights ---
echo "[TEST] Running test_c_decryptModelWeights (using test/client/config/test_c_config.json)..."
./test/client/build/test_c_decryptModelWeights --config test/client/config/test_c_config.json

echo "All tests completed successfully."

