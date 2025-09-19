#!/bin/bash
# Helper Functions for PPFL, used by server/client/comm

# =========
# - lightweight communication metrics logging (CSV)
# =========

# If BASE_DIR is not set by the caller (run.sh sets it), try to infer it
if [ -z "$BASE_DIR" ]; then
  BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fi


# Metrics file (one CSV for client-side instrumentation)
METRICS_DIR="$BASE_DIR/orchestration/metrics"
METRICS_FILE="$METRICS_DIR/comm_metrics.csv"
mkdir -p "$METRICS_DIR"

# header: timestamp,role,method,endpoint,client_id,type,file,payload_size,bytes_sent,bytes_received,latency_ms,http_code

if [ ! -s "$METRICS_FILE" ]; then
  echo "timestamp,role,method,endpoint,client_id,type,file,payload_size,bytes_sent,bytes_received,latency_ms,http_code" > "$METRICS_FILE"
fi

# =========
# msend: thin wrapper around curl for GET/POST used by comm layer
# Usage:
#   msend GET  <url> <output>
#   msend POST <url> <output> <client_id> <type> <file>
# =========

# msend: thin wrapper around curl for GET/POST used by comm layer
msend() {
    local method=$1     # GET or POST
    local url=$2        # full URL (http://ip:port/endpoint)
    local output=$3     # destination file (for GET response or POST ack)
    local client_id=$4  # client id (optional, used in POST)
    local type=$5       # type of upload (pubkey, rekey, weights, etc.)
    local file=$6       # file path to send in POST
    
    # Extract endpoint for logging
    local endpoint
    endpoint="$(echo "$url" | sed -E 's|https?://[^/]+||')"
    [ -z "$endpoint" ] && endpoint="/"

    # Start timestamp
    local start_ts=$(date +%s%3N)

    local http_code=""
    local rc=0

    if [ "$method" = "GET" ]; then
        echo "[helper_fns.sh] GET -> $url"
        mkdir -p "$(dirname "$output")" 2>/dev/null || true

        for i in {1..5}; do
            http_code="$(curl -f -s -S -w "%{http_code}" -o "$output" "$url" 2>/dev/null)"
            rc=$?
            [ $rc -eq 0 ] && break
            echo "[helper_fns.sh] WARN: GET failed (attempt $i), retrying..."
            sleep 1
        done

        local bytes_received=0
        [ -f "$output" ] && bytes_received=$(stat -c%s "$output" 2>/dev/null || echo 0)

        local end_ts=$(date +%s%3N)
        local latency_ms=$((end_ts - start_ts))
        local bytes_sent=512   # small request headers
        local payload_size=0   # no upload payload

        echo "$(date '+%Y-%m-%d %H:%M:%S'),client,GET,${endpoint},${client_id},${type},${output},${payload_size},${bytes_sent},${bytes_received},${latency_ms},${http_code}" \
            >> "$METRICS_FILE"

        [ $rc -ne 0 ] && { echo "[helper_fns.sh] ERROR: curl GET failed ($url)"; return 1; }

    elif [ "$method" = "POST" ]; then
        if [ -z "$file" ] || [ ! -f "$file" ]; then
            echo "[helper_fns.sh] ERROR: File to upload missing: $file"
            return 1
        fi
        echo "[helper_fns.sh] POST -> $url (client_id=$client_id, type=$type, file=$file)"

        http_code="$(curl -s -S -w "%{http_code}" -o "$output" -X POST "$url" \
            -F "file=@${file}" \
            -F "client_id=${client_id}" \
            -F "type=${type}" 2>/dev/null)"
        rc=$?

        local end_ts=$(date +%s%3N)
        local latency_ms=$((end_ts - start_ts))

        local payload_size=$(stat -c%s "$file" 2>/dev/null || echo 0)
        local bytes_sent=$((payload_size + 1024)) # file + overhead
        local bytes_received=0
        [ -f "$output" ] && bytes_received=$(stat -c%s "$output" 2>/dev/null || echo 0)

        echo "$(date '+%Y-%m-%d %H:%M:%S'),client,POST,${endpoint},${client_id},${type},${file},${payload_size},${bytes_sent},${bytes_received},${latency_ms},${http_code}" \
            >> "$METRICS_FILE"

        [ $rc -ne 0 ] && { echo "[helper_fns.sh] ERROR: curl POST failed ($url)"; return 1; }

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
