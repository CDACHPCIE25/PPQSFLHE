// server/src/runMserver.cpp
// Runs a Mongoose HTTP server to serve CC.json from server/storage, configured via sConfig.json

#define MG_MAX_RECV_SIZE (100UL * 1024 * 1024) // 100 MB - Increased receive buffer size for Mongoose as default is 3 MB

#include "../../lib/mongoose/mongoose.h" // Mongoose HTTP library for web server
#include <string>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp> // JSON parsing for configuration

using json = nlohmann::json; // Alias for nlohmann::json

// --- Utility functions --- 
// Compares Mongoose string (mg_str) with C-string for equality
// Parameters: a (Mongoose string), b (C-string)
// Returns: 0 if equal, -1 if lengths differ or content mismatches
static int mg_vcmp(const struct mg_str *a, const char *b) {
    size_t blen = strlen(b);
    if (a->len != blen) return -1;
    return memcmp(a->buf, b, blen);
}

// Checks if incoming URI matches expected path
// Parameters: uri (Mongoose URI string), path (expected path)
// Returns: true if URI matches path, false otherwise
static bool is_uri_equal(const struct mg_str uri, const char *path) {
    size_t len = strlen(path);
    return (uri.len == len && memcmp(uri.buf, path, len) == 0);
}

// Server configuration structure to hold IP, port, and CC.json path
// --- Config ---
struct ServerConfig {
    std::string ip;
    int port;
    std::string cc_path;
};

// Loads server configuration from sConfig.json
// Parameters: config_path (path to sConfig.json)
// Returns: ServerConfig with parsed IP, port, and cc_path
// Throws: std::runtime_error if file cannot be opened or parsed
static ServerConfig load_config(const std::string &config_path) {
    std::ifstream f(config_path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path);
    }
    json j;
    f >> j;

    ServerConfig cfg;
    cfg.ip = j["mSConfig"]["SERVER_IP"].get<std::string>();
    cfg.port = j["mSConfig"]["SERVER_PORT"].get<int>();
    cfg.cc_path = j["CC"]["path"].get<std::string>();
    return cfg;
}

// --- Handlers ---
static void handle_getCC(struct mg_connection *c, struct mg_http_message *hm, const std::string &cc_path) {
    std::cout << "[SERVER] Serving " << cc_path << std::endl;
    struct mg_http_serve_opts opts = {0};
    mg_http_serve_file(c, hm, cc_path.c_str(), &opts);
}

static void handle_sendPbKey(struct mg_connection *c, struct mg_http_message *hm) {
    // Placeholder for now
    mg_http_reply(c, 200, "", "sendPbKey handler not yet implemented\n");
}

// Handles HTTP requests, serving CC.json on GET /getCC or returning 404
// Parameters: c (Mongoose connection), ev (event type), ev_data (event data), cc_path (CC.json path)
// --- Router ---
static void handle_request(struct mg_connection *c, int ev, void *ev_data, const std::string &cc_path) {
    if (ev != MG_EV_HTTP_MSG) return;
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    if (is_uri_equal(hm->uri, "/getCC") && mg_vcmp(&hm->method, "GET") == 0) {
        handle_getCC(c, hm, cc_path);

    } else if (is_uri_equal(hm->uri, "/sendPbKey") && mg_vcmp(&hm->method, "GET") == 0) {
        handle_sendPbKey(c, hm);

    } else {
        mg_http_reply(c, 404, "", "Not found\n"); // Return 404 for invalid routes
    }
}

// Static adapter to pass cc_path via fn_data to handle_request
// Parameters: c (Mongoose connection), ev (event type), ev_data (event data)
// Note: fn_data is passed by mg_http_listen and contains &cfg.cc_path
// --- Adapter for Mongoose ---
static void event_handler(struct mg_connection *c, int ev, void *ev_data) {
    auto *cc_path = static_cast<std::string *>(c->fn_data); // Access fn_data from connection
    handle_request(c, ev, ev_data, *cc_path); // Call handler with cc_path
}


// Main function: Starts Mongoose server with configuration from sConfig.json
// --- Main ---
int main() {
    try {
        ServerConfig cfg = load_config("server/config/sConfig.json"); // Load configuration from sConfig.json

        struct mg_mgr mgr; // Initialize Mongoose event manager
        mg_mgr_init(&mgr);

        std::string url = "http://" + cfg.ip + ":" + std::to_string(cfg.port); // Construct server URL from config
        struct mg_connection *c = mg_http_listen(&mgr, url.c_str(), event_handler, &cfg.cc_path); // Start HTTP listener with event_handler, passing cc_path as fn_data

        if (!c) {
            std::cerr << "[SERVER] Failed to start server on " << url << std::endl;
            return 1;
        }

        std::cout << "[SERVER] Mongoose HTTP server running on " << url << std::endl;

        for (;;) mg_mgr_poll(&mgr, 1000); // Poll for events indefinitely
        mg_mgr_free(&mgr); // Cleanup

    } catch (const std::exception &e) {
        std::cerr << "[SERVER] ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
