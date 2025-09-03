# ==========================
# Communication Wrappers
# - All file-transfer / distribution helpers
# - Uses CLIENT_* and SERVER_* variables
# ==========================

# comm_getCC: client fetches CC.json (either via HTTP or local copy)
comm_getCC() {
    local client_id=$1
    local dest=$2
    log "comm" "getCC" "Client $client_id fetching CC.json -> $dest"
    #echo "[comm] Client $client_id getting CC.json..."

    if [ "$COMM_MODE" = "MONGOOSE" ]; then
        msend GET "http://${SERVER_IP}:${SERVER_PORT}/getCC" "$dest"
    else
        cp "$SERVER_STORAGE_DIR/CC.json" "$dest"
    fi

    if [ ! -f "$dest" ]; then
        log "comm" "error" "Client $client_id failed to get CC.json" 
        #echo "[comm] ERROR: Client $client_id failed to get CC.json"
        exit 1
    fi
}

# comm_sendKey: generic reliable copy/transfer wrapper (placeholder POST vs local copy)
comm_sendKey() {
    local src=$1
    local dest=$2
    local desc=$3

    if [ "$COMM_MODE" = "MONGOOSE" ]; then
        cp "$src" "$dest"  # placeholder, could POST later
    else
        cp "$src" "$dest"
    fi
    log "comm" "send" "Transferred $desc ($src -> $dest)"
    #echo "[comm] Sent $desc"
}

# ==========================
# Orchestration Helpers
# ==========================

# o_send_cc_to_clients: send CC.json produced on server to each client
s_send_cc_to_c() {
    comm_getCC 1 "$CLIENT_1_STORAGE/CC.json"
    comm_getCC 2 "$CLIENT_2_STORAGE/CC.json"
}

# --- Send client public keys to server ---
c_send_pubkeys_to_s() {
    comm_sendKey "$CLIENT_1_PUBKEY" "$SERVER_STORAGE_DIR/client_1/client_1-public.key" "Client 1 pubkey to server"
    comm_sendKey "$CLIENT_2_PUBKEY" "$SERVER_STORAGE_DIR/client_2/client_2-public.key" "Client 2 pubkey to server"

}

# --- Distribute pubkeys among clients ---
s_send_pubkeys_to_c(){
    comm_sendKey "$SERVER_STORAGE_DIR/client_1/client_1-public.key" "$CLIENT_2_STORAGE/client_1-public.key" "Client 1 pubkey to Client 2"
    comm_sendKey "$SERVER_STORAGE_DIR/client_2/client_2-public.key" "$CLIENT_1_STORAGE/client_2-public.key" "Client 2 pubkey to Client 1"
}

# --- Send client Rekeys to server ---
c_Rekeys_to_s() {
    comm_sendKey "$CLIENT_1_REKEY" "$SERVER_STORAGE_DIR/client_1/client_1-ReKey.key" "Client 1 rekey to server"
    comm_sendKey "$CLIENT_2_REKEY" "$SERVER_STORAGE_DIR/client_2/client_2-ReKey.key" "Client 2 rekey to server"
}

# Communicate encrypted weights from clients to server
c_sends_encrypted_weights_to_s() {
    comm_sendKey "$CLIENT_1_ENCWEIGHTS" "$SERVER_STORAGE_DIR/client_1/encrypted_weights_c1.json" "Client 1 encrypted weights to server"
    comm_sendKey "$CLIENT_2_ENCWEIGHTS" "$SERVER_STORAGE_DIR/client_2/encrypted_weights_c2.json" "Client 2 encrypted weights to server"
}

# o_send_aggregated_to_clients: server -> clients aggregated files (post-aggregation)
s_send_aggregated_to_c() {
    comm_sendKey "$reenc_c2_c1" "$CLIENT_1_AGGRENCWEIGHTS" "Aggregated weights to Client 1"
    comm_sendKey "$aggrencfile" "$CLIENT_2_AGGRENCWEIGHTS" "Aggregated weights to Client 2"
}
