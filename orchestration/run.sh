#!/bin/bash
# =====================================
# PPFL Orchestration
# =====================================

set -e  # Exit if any command fails

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$SCRIPT_DIR/.."

# ---- Source modular functions ----
source "$SCRIPT_DIR/helper_fns.sh" # defines READJSON, msend
source "$SCRIPT_DIR/server_fns.sh" # defines SERVER_STORAGE_DIR and server-side bin paths
source "$SCRIPT_DIR/client_fns.sh" # defines client storage paths and client-side macros
source "$SCRIPT_DIR/comm_fns.sh"   # comm_fns uses both server and client path vars and provides transfer wrappers

# ---- Load Orchestration Config ----
ORCH_CONFIG="$SCRIPT_DIR/oConfig.json"
COMM_MODE=$(jq -r '.orchestration.COMM_MODE' "$ORCH_CONFIG")
ROUNDS=$(jq -r '.orchestration.ROUNDS' "$ORCH_CONFIG")
SERVER_IP=$(jq -r '.orchestration.SERVER_IP' "$ORCH_CONFIG")
SERVER_PORT=$(jq -r '.orchestration.SERVER_PORT' "$ORCH_CONFIG")


# ============================================================
# Round Execution
# ============================================================
run_round() {
    local round=$1
    log "orchestrator" "round" "Executing Round $round"
    #echo "[round] Executing Round $round"
  
    # --- Local training/update for each client ---
    c_training
    
    # Orchestration sequence
    c_encryptWeights # clients encrypt local weights 
    c_sends_encrypted_weights_to_s # orchestrator: sends encrypted weights to server
    s_changeCipherDomain_c1_c2    # server: convert c1 -> c2 domain
    s_aggregateEncryptedWeights
    s_changeCipherDomain_c2_c1    # server: convert c2 -> c1 domain
    s_send_aggregated_to_c  # orchestrator: sends aggregated weights to clients
    c_decryptWeights              # clients decrypt final aggregated weights
}

# ============================================================
# Main Orchestration
# ============================================================
main() {
    log "orchestrator" "Starting Orchestration"
    #echo "[orchestrator] === Starting Orchestration ==="
    
    #----Init phase----
    
    s_genCC              # produce CC.json on server
    s_Mserver            # start Mongoose server
    s_send_cc_to_c       # send CC.json to clients
    c_keyGen             # clients generate key pair
    c_send_pubkeys_to_s  # orchestrator send pubkeys to server
    s_send_pubkeys_to_c  # orchestrator distributes pubkeys among clients
    c_RekeyGen           # clients produce rekeys
    c_Rekeys_to_s        # orchestrator send rekeys to server

    # ----- Training Rounds -----
    for (( r=1; r<=ROUNDS; r++ )); do
        log "orchestrator" "round" "===== ROUND $r / $ROUNDS ====="
        run_round "$r"
    done

    s_stop_Mserver

    log "orchestrator" "Orchestration Completed"
    #echo "[orchestrator] === Orchestration Completed ==="
}

main "$@"
