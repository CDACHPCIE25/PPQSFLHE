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
# - POST via curl when in MONGOOSE mode
# - cp otherwise
comm_sendKey() {
    local src=$1         # source file
    local dest=$2        # destination (for cp mode only)
    local desc=$3        # description (for logs)
    local client_id=$4
    local endpoint=$5    # e.g., uploadPubKeyC1, uploadPubKeyC2
    local type=$6        # NEW: pubkey, rekey, weights

    if [ "$COMM_MODE" = "MONGOOSE" ]; then
        local tmpout=$(mktemp)
        msend POST "http://${SERVER_IP}:${SERVER_PORT}/${endpoint}" "$tmpout" "$client_id" "$type" "$src"
        rm -f "$tmpout"
    else
        cp "$src" "$dest"
    fi
    log "comm" "send" "Transferred $desc ($src -> $dest)"
}

# Fetch an arbitrary file from the server's server/storage/<relpath>
# $1 = server-relative path under server/storage (e.g. "client_2/client_2-public.key")
# $2 = local destination filename (full path where client will write file)
comm_getFile() {
    local relpath=$1
    local dest=$2

    log "comm" "getFile" "Fetching ${relpath} from server -> ${dest}"

    # Ensure relpath doesn't start with a leading slash
    relpath="${relpath#/}"

    # Use the msend GET wrapper so it benefits from retries and consistent logging
    msend GET "http://${SERVER_IP}:${SERVER_PORT}/download/${relpath}" "$dest"
    if [ $? -ne 0 ]; then
        log "comm" "error" "Failed fetching ${relpath} -> ${dest}"
        exit 1
    fi

    # sanity check
    if [ ! -f "$dest" ]; then
        log "comm" "error" "Downloaded file missing: $dest"
        exit 1
    fi

    log "comm" "getFile" "Successfully fetched ${relpath} -> ${dest}"
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
    if [ "$COMM_MODE" = "MONGOOSE" ]; then
        comm_sendKey "$CLIENT_1_PUBKEY" "" "Client 1 pubkey to server" 1 "uploadPubKeyC1" "pubkey"
        comm_sendKey "$CLIENT_2_PUBKEY" "" "Client 2 pubkey to server" 2 "uploadPubKeyC2" "pubkey"
    else
        comm_sendKey "$CLIENT_1_PUBKEY" "$SERVER_STORAGE_DIR/client_1/client_1-public.key" "Client 1 pubkey to server"
        comm_sendKey "$CLIENT_2_PUBKEY" "$SERVER_STORAGE_DIR/client_2/client_2-public.key" "Client 2 pubkey to server"
    fi
}

# --- Distribute pubkeys among clients ---
s_send_pubkeys_to_c() {
    if [ "$COMM_MODE" = "MONGOOSE" ]; then
        # Client 2 should fetch Client 1's public key from server/storage/client_1/client_1-public.key
        comm_getFile "client_1/client_1-public.key" "$CLIENT_2_STORAGE/client_1-public.key"

        # Client 1 should fetch Client 2's public key from server/storage/client_2/client_2-public.key
        comm_getFile "client_2/client_2-public.key" "$CLIENT_1_STORAGE/client_2-public.key"
    else
        comm_sendKey "$SERVER_STORAGE_DIR/client_1/client_1-public.key" "$CLIENT_2_STORAGE/client_1-public.key" "Client 1 pubkey to Client 2"
        comm_sendKey "$SERVER_STORAGE_DIR/client_2/client_2-public.key" "$CLIENT_1_STORAGE/client_2-public.key" "Client 2 pubkey to Client 1"
    fi
}

# --- Send client Rekeys to server ---
c_Rekeys_to_s() {
    if [ "$COMM_MODE" = "MONGOOSE" ]; then
        comm_sendKey "$CLIENT_1_REKEY" "" "Client 1 rekey to server" 1 "uploadReKeyC1" "rekey"
        comm_sendKey "$CLIENT_2_REKEY" "" "Client 2 rekey to server" 2 "uploadReKeyC2" "rekey"
    else
        comm_sendKey "$CLIENT_1_REKEY" "$SERVER_STORAGE_DIR/client_1/client_1-ReKey.key" "Client 1 rekey to server"
        comm_sendKey "$CLIENT_2_REKEY" "$SERVER_STORAGE_DIR/client_2/client_2-ReKey.key" "Client 2 rekey to server"
    fi
}

# Communicate encrypted weights from clients to server
c_sends_encrypted_weights_to_s() {
    if [ "$COMM_MODE" = "MONGOOSE" ]; then
        comm_sendKey "$CLIENT_1_ENCWEIGHTS" "" "Client 1 encrypted weights to server" 1 "uploadEncWeightsC1" "weights"
        comm_sendKey "$CLIENT_2_ENCWEIGHTS" "" "Client 2 encrypted weights to server" 2 "uploadEncWeightsC2" "weights"
    else
        comm_sendKey "$CLIENT_1_ENCWEIGHTS" "$SERVER_STORAGE_DIR/client_1/encrypted_weights_c1.json" "Client 1 encrypted weights to server"
        comm_sendKey "$CLIENT_2_ENCWEIGHTS" "$SERVER_STORAGE_DIR/client_2/encrypted_weights_c2.json" "Client 2 encrypted weights to server"
    fi
}

# o_send_aggregated_to_clients: server -> clients aggregated files (post-aggregation)
s_send_aggregated_to_c() {
  if [ "$COMM_MODE" = "MONGOOSE" ]; then
    # Client 1 should fetch re-encrypted (c2->c1) aggregate
    comm_getFile "client_1/c2_domainChange_c1.json" "$CLIENT_1_AGGRENCWEIGHTS/c2_domainChange_c1.json"

    # Client 2 should fetch aggregated ciphertexts
    comm_getFile "client_2/aggregated_weights.json" "$CLIENT_2_AGGRENCWEIGHTS/aggregated_weights.json"
  else
    cp "$reenc_c2_c1" "$CLIENT_1_AGGRENCWEIGHTS/c2_domainChange_c1.json"
    cp "$aggrencfile" "$CLIENT_2_AGGRENCWEIGHTS/aggregated_weights.json"
  fi
}
