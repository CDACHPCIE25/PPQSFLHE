#!/bin/bash
# ==========================
# Client Functions, Client binaries, storage paths and client-side compute actions
# ==========================

CLIENT_BUILD="$BASE_DIR/client/build"
KEYGEN_BIN="$CLIENT_BUILD/keyGen"
REKEYGEN_BIN="$CLIENT_BUILD/REkeyGen"
ENCRYPT_BIN="$CLIENT_BUILD/encryptModelWeights"
DECRYPT_BIN="$CLIENT_BUILD/decryptModelWeights"

CLIENT_1_STORAGE="$BASE_DIR/client/storage/client_1/public"
CLIENT_2_STORAGE="$BASE_DIR/client/storage/client_2/public"

CLIENT_1_PUBKEY="$CLIENT_1_STORAGE/client_1-public.key"
CLIENT_2_PUBKEY="$CLIENT_2_STORAGE/client_2-public.key"
CLIENT_1_REKEY="$CLIENT_1_STORAGE/client_1-ReKey.key"
CLIENT_2_REKEY="$CLIENT_2_STORAGE/client_2-ReKey.key"

CLIENT_1_ENCWEIGHTS="$CLIENT_1_STORAGE/../private/encrypted_weights_c1.json"
CLIENT_2_ENCWEIGHTS="$CLIENT_2_STORAGE/../private/encrypted_weights_c2.json"

CLIENT_1_AGGRENCWEIGHTS="$CLIENT_1_STORAGE/../private/"
CLIENT_2_AGGRENCWEIGHTS="$CLIENT_2_STORAGE/../private/"

o_get_cc_for_clients() {
    comm_getCC 1 "$CLIENT_1_STORAGE/CC.json"
    comm_getCC 2 "$CLIENT_2_STORAGE/CC.json"
}

# ----------------------------
# Client compute actions, across all clients
# ----------------------------

# c_keygen: Each client generates its key pair (writes PUBKEY and PRIVKEY)
c_keyGen() {
    for i in 1 2; do
        CLIENT_CONFIG="$BASE_DIR/client/config/client_$i/c_config.json"
        cc_path=$(READJSON "$CLIENT_CONFIG" '.CLIENT.CC_PATH')
        pubkey_out=$(READJSON "$CLIENT_CONFIG" '.CLIENT.PUBKEY_PATH')
        privkey_out=$(READJSON "$CLIENT_CONFIG" '.CLIENT.PRIVKEY_PATH')

        log "client_$i" "KeyGen" "Generating key pair"
        #echo "[client] keyGen for Client $i..."
        "$KEYGEN_BIN" "$cc_path" "$pubkey_out" "$privkey_out"
    done
}

# c_Rekeygen: each client generates a re-key for server domain changes
c_RekeyGen() {
    for i in 1 2; do
        CLIENT_CONFIG="$BASE_DIR/client/config/client_$i/c_config.json"
        cc_path=$(READJSON "$CLIENT_CONFIG" '.CLIENT.CC_PATH')
        privkey=$(READJSON "$CLIENT_CONFIG" '.CLIENT.PRIVKEY_PATH')
        peer_pubkey=$(READJSON "$CLIENT_CONFIG" '.CLIENT.PEER_PUBKEY_PATH')
        rekey_out=$(READJSON "$CLIENT_CONFIG" '.CLIENT.REKEY_PATH')

        log "client_$i" "ReKeyGen" "Generating for domain change"
        #echo "[client] REkeyGen for Client $i..."
        "$REKEYGEN_BIN" "$cc_path" "$privkey" "$peer_pubkey" "$rekey_out"
    done
}

# c_training: Run local training (calls Python training script for each client)
c_training() {
    for i in 1 2; do
        CLIENT_CONFIG="$BASE_DIR/client/config/client_$i/c_config.json"
        log "client_$i" "c_training" "Local Training"
        #echo "[client $i] Local training..."
        python3 "$BASE_DIR/client/src/c_trainAndUpdate.py" "$CLIENT_CONFIG"
    done
}

# c_encryptWeights: clients encrypt local weights (produces encrypted files)
c_encryptWeights() {
    for i in 1 2; do
        CLIENT_CONFIG="$BASE_DIR/client/config/client_$i/c_config.json"
        cc_path=$(READJSON "$CLIENT_CONFIG" '.CLIENT.CC_PATH')
        pubkey=$(READJSON "$CLIENT_CONFIG" '.CLIENT.PUBKEY_PATH')
        inputweights=$(READJSON "$CLIENT_CONFIG" '.CLIENT.INPUT_WEIGHTS_PATH')
        outputencfile=$(READJSON "$CLIENT_CONFIG" '.CLIENT.OUTPUT_ENCRYPTED_WEIGHTS_PATH')

        log "client_$i" "c_encryptWeights" "Encrypting local weights"
        #echo "[client] Encrypting weights for Client $i..."
        "$ENCRYPT_BIN" "$cc_path" "$pubkey" "$inputweights" "$outputencfile"
    done
}

#c_decryptWeights: clients decrypt the aggregated ciphertext into plaintext
c_decryptWeights() {
    for i in 1 2; do
        CLIENT_CONFIG="$BASE_DIR/client/config/client_$i/c_config.json"
        cc_path=$(READJSON "$CLIENT_CONFIG" '.CLIENT.CC_PATH')
        privkey=$(READJSON "$CLIENT_CONFIG" '.CLIENT.PRIVKEY_PATH')
        inputweights=$(READJSON "$CLIENT_CONFIG" '.CLIENT.AGGREGATED_ENCRYPTED_WEIGHTS_PATH')
        outputdecfile=$(READJSON "$CLIENT_CONFIG" '.CLIENT.OUTPUT_DECRYPTED_WEIGHTS_PATH')

        log "client_$i" "c_decryptWeights" "Decrypting Aggregated weights"
        #echo "[client] Decrypting aggregated weights for Client $i..."
        "$DECRYPT_BIN" "$cc_path" "$privkey" "$inputweights" "$outputdecfile"
    done
}



