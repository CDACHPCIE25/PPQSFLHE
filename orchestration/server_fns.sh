#!/bin/bash
# ==========================
# Server Functions, Server binaries, storage paths, and server-side actions
# ==========================

# Load server networking config
#SERVER_IP=$(jq -r '.mSConfig.SERVER_IP' "$SERVER_CONFIG")
#SERVER_PORT=$(jq -r '.mSConfig.SERVER_PORT' "$SERVER_CONFIG")

SERVER_BUILD="$BASE_DIR/server/build"
SERVER_STORAGE_DIR="$BASE_DIR/server/storage"
SERVER_CONFIG="$BASE_DIR/server/config/sConfig.json"

GENCC_BIN="$SERVER_BUILD/genCC"
RUNMSERVER_BIN="$SERVER_BUILD/runMserver"
CHANGECIPHER_BIN="$SERVER_BUILD/changeCipherDomain"
AGGREGATE_BIN="$SERVER_BUILD/aggregateEncryptedWeights"

# Load server config values used by server actions (read at source time)
cc_path=$(READJSON "$SERVER_CONFIG" '.CC.path')
enc_c1=$(READJSON "$SERVER_CONFIG" '.CLIENTS.CLIENT_1_ENCRYPTED_WEIGHTS_PATH')
rekey_c1=$(READJSON "$SERVER_CONFIG" '.CLIENTS.CLIENT_1_REKEY')
reenc_c1_c2=$(READJSON "$SERVER_CONFIG" '.CLIENTS.OUTPUT_DOMAIN_CHANGED_PATH')
enc_c2=$(READJSON "$SERVER_CONFIG" '.CLIENTS.CLIENT_2_ENCRYPTED_WEIGHTS_PATH')
aggrencfile=$(READJSON "$SERVER_CONFIG" '.CLIENTS.AGGREGATED_ENCRYPTED_WEIGHTS_PATH')
rekey_c2=$(READJSON "$SERVER_CONFIG" '.CLIENTS.CLIENT_2_REKEY')
reenc_c2_c1=$(READJSON "$SERVER_CONFIG" '.CLIENTS.OUTPUT_AGGREGATED_DOMAIN_CHANGED_PATH')

# ----------------------------
# Server actions
# ----------------------------

# s_genCC: create cryptocontext (CC.json) on server storage
s_genCC() {
    log "server" "Running genCC"
    #echo "[server] Running genCC..."
    "$GENCC_BIN"
    [ -f "$SERVER_STORAGE_DIR/CC.json" ] || {
        echo "[server] ERROR: genCC failed"
        exit 1
    }
}

# s_Mserver: start the mongoose server
s_Mserver() {
    log "server" "Starting Mserver"
    #echo "[server] Starting Mserver..."
    pkill -f "runMserver" || true
    "$RUNMSERVER_BIN" &
    SERVER_PID=$!
    sleep 2
}

# o_stop_server: stop the started server (uses SERVER_PID)
s_stop_Mserver() {
    log "server" "Stopping Mserver"
    #echo "[server] Stopping runMserver..."
    kill $SERVER_PID
}

# s_changeCipherDomain_c1_to_c2: change cipher domain from c1 -> c2
s_changeCipherDomain_c1_c2() {
    log "server" "changeCipherDomain (C1->C2)..."
    #echo "[server] changeCipherDomain (C1->C2)..."
    "$CHANGECIPHER_BIN" "$cc_path" "$rekey_c1" "$enc_c1" "$reenc_c1_c2"
}

# s_aggregateEncryptedWeights: aggregate ciphertexts (server-side aggregator)
s_aggregateEncryptedWeights() {
    log "server" "aggregateEncryptedWeights..."
    #echo "[server] aggregateEncryptedWeights..."
    "$AGGREGATE_BIN" "$cc_path" "$enc_c2" "$reenc_c1_c2" "$aggrencfile"
}

# s_changeCipherDomain_c2_to_c1: convert aggregated c2 domain to c1 domain
s_changeCipherDomain_c2_c1() {
    log "server" "changeCipherDomain (C2->C1)..."
    #echo "[server] changeCipherDomain (C2->C1)..."
    "$CHANGECIPHER_BIN" "$cc_path" "$rekey_c2" "$aggrencfile" "$reenc_c2_c1"
}
