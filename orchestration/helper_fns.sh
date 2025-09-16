#!/bin/bash
# =========
# Helper Functions for PPFL, used by server/client/comm
# =========

# msend: thin wrapper around curl for GET/POST used by comm layer
msend() {
    local method=$1     # GET or POST
    local url=$2        # full URL (http://ip:port/endpoint)
    local output=$3     # destination file (for GET response or POST ack)
    local client_id=$4  # client id (optional, used in POST)
    local type=$5       # type of upload (pubkey, rekey, weights, etc.)
    local file=$6       # file path to send in POST

    if [ "$method" = "GET" ]; then
        echo "[helper_fns.sh] GET -> $url"
        for i in {1..5}; do
            curl -f -s -S -o "$output" "$url" && break
            echo "[helper_fns.sh] WARN: GET failed (attempt $i), retrying..."
            sleep 1
        done

    elif [ "$method" = "POST" ]; then
        if [ -z "$file" ] || [ ! -f "$file" ]; then
            echo "[helper_fns.sh] ERROR: File to upload missing: $file"
            return 1
        fi
        echo "[helper_fns.sh] POST -> $url (client_id=$client_id, type=$type, file=$file)"

        curl -s -X POST "$url" \
             -F "file=@${file}" \
             -F "client_id=${client_id}" \
             -F "type=${type}" \
             -o "$output"
        if [ $? -ne 0 ]; then
            echo "[helper_fns.sh] ERROR: curl POST failed ($url)"
            return 1
        fi

    else
        echo "[helper_fns.sh] ERROR: Invalid msend method: $method"
        return 1
    fi

    echo "[helper_fns.sh] SUCCESS: $method completed"
    return 0
}


# Wrapper to read JSON values safely, jq wrapper returning a value or exiting with an error
READJSON() {
    local config_file=$1 # First argument = path to JSON file
    local key=$2 ## Second argument = jq key filter (e.g., '.CLIENT.CC_PATH')
    if [ ! -f "$config_file" ]; then 
        echo "[helper_fns.sh] ERROR: Config file $config_file not found"
        exit 1
    fi
    local value
    value=$(jq -r "$key" "$config_file" 2>/dev/null)
    # jq -r "$key" "$config_file" runs jq with:
      # -r ? raw output (so "abc" becomes abc, not quoted).
      # $key ? the query filter (like .CLIENT.CC_PATH).
      # $config_file ? the JSON file to parse.
    # 2>/dev/null ? suppresses error messages if jq fails.
    
    # This captures the JSON value (like /home/user/CC.json) into value
    if [ $? -ne 0 ] || [ "$value" = "null" ] || [ -z "$value" ]; then
        echo "[helper_fns.sh] ERROR: Failed to read key $key from $config_file"
        exit 1
    fi
    echo "$value" #If all good, prints the value. shell functions return values by echo
    
    # the caller captures it in a variable like: path=$(READJSON client/config/client_1/c_config.json '.CLIENT.CC_PATH')
}


# Log function for Structure Logging
log() {
    local role=$1   # e.g., orchestrator, client_1, server
    local step=$2   # e.g., training, encrypt, aggregate
    local msg=$3    # freeform message
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [${role}] [${step}] $msg"
}
