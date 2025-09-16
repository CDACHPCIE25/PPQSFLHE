// server/src/runMserver.cpp
// Mongoose HTTP server to serve CC.json and handle public key uploads
// Compatible with pre-7.x Mongoose

#define MG_MAX_HTTP_BODY_SIZE (100UL * 1024 * 1024) // 100 MB
#define MG_MAX_RECV_SIZE (200ULL * 1024ULL * 1024ULL) // 200 MB
#define MG_IO_SIZE (3 * 1024 * 1024) // 3 MB chunk size

#include "../../lib/mongoose/mongoose.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

// --- Utility Functions ---
static int mg_vcmp(const struct mg_str *a, const char *b) {
    size_t blen = strlen(b);
    if (a->len != blen) return -1;
    return memcmp(a->buf, b, blen);
}

static bool is_uri_equal(const struct mg_str uri, const char *path) {
    size_t len = strlen(path);
    return (uri.len == len && memcmp(uri.buf, path, len) == 0);
}

// --- Server Configuration ---
struct ServerConfig {
    std::string ip;
    int port;
    std::string cc_path;
    std::string pubkey_path_client1;
    std::string pubkey_path_client2;
    std::string rekey_path_client1;
    std::string rekey_path_client2;
    
    std::string client_1_enc_w_p;
    std::string client_2_enc_w_p;
    
    std::string output_domain_chg_p;
    
    std::string agg_w_p;
        
    std::string domain_chg_agg_w_p;
    
};

// Load configuration from JSON
static ServerConfig load_config(const std::string &config_path) {
    std::ifstream f(config_path);
    if (!f.is_open()) throw std::runtime_error("Cannot open config: " + config_path);

    json j;
    f >> j;
    ServerConfig cfg;
    cfg.ip = j["mSConfig"]["SERVER_IP"].get<std::string>();
    cfg.port = j["mSConfig"]["SERVER_PORT"].get<int>();
    cfg.cc_path = j["CC"]["path"].get<std::string>();
    cfg.pubkey_path_client1 = j["CLIENTS"]["CLIENT_1_PUBLIC"].get<std::string>();
    cfg.pubkey_path_client2 = j["CLIENTS"]["CLIENT_2_PUBLIC"].get<std::string>();
    cfg.rekey_path_client1 = j["CLIENTS"]["CLIENT_1_REKEY"].get<std::string>();
    cfg.rekey_path_client2 = j["CLIENTS"]["CLIENT_2_REKEY"].get<std::string>();
    
    cfg.client_1_enc_w_p = j["CLIENTS"]["CLIENT_1_ENCRYPTED_WEIGHTS_PATH"].get<std::string>();
    cfg.client_2_enc_w_p = j["CLIENTS"]["CLIENT_2_ENCRYPTED_WEIGHTS_PATH"].get<std::string>();
    
    cfg.output_domain_chg_p = j["CLIENTS"]["OUTPUT_DOMAIN_CHANGED_PATH"].get<std::string>();
    
    cfg.agg_w_p = j["CLIENTS"]["AGGREGATED_ENCRYPTED_WEIGHTS_PATH"].get<std::string>();
    
    cfg.domain_chg_agg_w_p = j["CLIENTS"]["OUTPUT_AGGREGATED_DOMAIN_CHANGED_PATH"].get<std::string>();
    
    return cfg;
}

// --- Handlers ---
static void handle_getCC(struct mg_connection *c, struct mg_http_message *hm, const std::string &cc_path) {
    std::cout << "[SERVER] Serving " << cc_path << std::endl;
    struct mg_http_serve_opts opts = {0};
    mg_http_serve_file(c, hm, cc_path.c_str(), &opts);
}

static void handle_sendPbKey(struct mg_connection *c, struct mg_http_message *hm, const std::string &pubkey_path) {
    std::ifstream f(pubkey_path, std::ios::binary);
    if (!f.is_open()) {
        mg_http_reply(c, 500, "", "Error: cannot open pubkey file\n");
        return;
    }
    std::ostringstream buffer;
    buffer << f.rdbuf();
    f.close();
    std::cout << "[SERVER] Serving Public Key from " << pubkey_path << std::endl;
    mg_http_reply(c, 200, "Content-Type: application/octet-stream\r\n", "%s", buffer.str().c_str());
}

// Buffered multipart upload (pre-7.x compatible)
static void handle_uploadPubKey(struct mg_connection *c, struct mg_http_message *hm, const std::string &dest_path) {
    if (mg_vcmp(&hm->method, "POST") != 0) {
        mg_http_reply(c, 405, "", "Method not allowed\n");
        return;
    }

    fs::path p(dest_path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());

    std::ofstream out(dest_path, std::ios::binary);
    if (!out.is_open()) {
        mg_http_reply(c, 500, "", "Error: cannot open file for writing\n");
        return;
    }

    struct mg_http_part part;
    size_t ofs = 0;
    while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0) {
        if (part.name.len > 0 && std::string(part.name.buf, part.name.len) == "file") {
            out.write(part.body.buf, part.body.len);
        }
    }
    out.close();

    std::cout << "[SERVER] Received and saved Public Key to " << dest_path << std::endl;
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\":\"received\"}");
}

// Serve files from server/storage/<client>/<filename>
static void handle_download(struct mg_connection *c, struct mg_http_message *hm, const ServerConfig &cfg) {
    std::string uri(hm->uri.buf, hm->uri.len);
    const std::string prefix = "/download/";
    std::string rel = uri.substr(prefix.size());

    fs::path base = fs::path(cfg.cc_path).parent_path(); // server/storage
    fs::path target = (base / rel).lexically_normal();

    if (!fs::exists(target) || !fs::is_regular_file(target)) {
        mg_http_reply(c, 404, "", "Not found\n");
        return;
    }

    std::ifstream f(target, std::ios::binary);
    if (!f.is_open()) {
        mg_http_reply(c, 500, "", "Failed to open file\n");
        return;
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string body = ss.str();

    std::ostringstream hdr;
    hdr << "Content-Type: application/octet-stream\r\n"
        << "Content-Length: " << body.size() << "\r\n";

    std::cout << "[SERVER] Serving file " << target << " (" << body.size() << " bytes)" << std::endl;
    mg_http_reply(c, 200, hdr.str().c_str(), "%s", body.c_str());
}

// --- Router ---
static void handle_request(struct mg_connection *c, int ev, void *ev_data, const ServerConfig &cfg) {
    if (ev != MG_EV_HTTP_MSG) return;
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    // --- GET endpoints ---
    if (is_uri_equal(hm->uri, "/getCC") && mg_vcmp(&hm->method, "GET") == 0) {
        handle_getCC(c, hm, cfg.cc_path);

    } else if (is_uri_equal(hm->uri, "/sendPbKeyC1") && mg_vcmp(&hm->method, "GET") == 0) {
        handle_sendPbKey(c, hm, cfg.pubkey_path_client1);

    } else if (is_uri_equal(hm->uri, "/sendPbKeyC2") && mg_vcmp(&hm->method, "GET") == 0) {
        handle_sendPbKey(c, hm, cfg.pubkey_path_client2);

    } else if (hm->uri.len > 10 && strncmp(hm->uri.buf, "/download/", 10) == 0) {
        handle_download(c, hm, cfg);

    // --- POST endpoints (uploads) ---
    } else if (is_uri_equal(hm->uri, "/uploadPubKeyC1") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.pubkey_path_client1);

    } else if (is_uri_equal(hm->uri, "/uploadPubKeyC2") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.pubkey_path_client2);

    } else if (is_uri_equal(hm->uri, "/uploadReKeyC1") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.rekey_path_client1);

    } else if (is_uri_equal(hm->uri, "/uploadReKeyC2") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.rekey_path_client2);

    } else if (is_uri_equal(hm->uri, "/uploadEncWeightsC1") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.client_1_enc_w_p);

    } else if (is_uri_equal(hm->uri, "/uploadEncWeightsC2") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.client_2_enc_w_p);

    } else if (is_uri_equal(hm->uri, "/uploadDomainChange") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.output_domain_chg_p);

    } else if (is_uri_equal(hm->uri, "/uploadAggregated") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.agg_w_p);

    } else if (is_uri_equal(hm->uri, "/uploadDomainChangeAgg") && mg_vcmp(&hm->method, "POST") == 0) {
        handle_uploadPubKey(c, hm, cfg.domain_chg_agg_w_p);

    } else {
        mg_http_reply(c, 404, "", "Not found\n");
    }
}

// Adapter for Mongoose
static void event_handler(struct mg_connection *c, int ev, void *ev_data) {
    auto *cfg = static_cast<ServerConfig *>(c->fn_data);
    handle_request(c, ev, ev_data, *cfg);
}

// --- Main ---
int main() {
    try {
        ServerConfig cfg = load_config("server/config/sConfig.json");

        struct mg_mgr mgr;
        mg_mgr_init(&mgr);

        std::string url = "http://" + cfg.ip + ":" + std::to_string(cfg.port);
        struct mg_connection *c = mg_http_listen(&mgr, url.c_str(), event_handler, &cfg);
        if (!c) {
            std::cerr << "[SERVER] Failed to start server on " << url << std::endl;
            return 1;
        }

        std::cout << "[SERVER] Mongoose HTTP server running on " << url << std::endl;

        for (;;) mg_mgr_poll(&mgr, 1000);
        mg_mgr_free(&mgr);

    } catch (const std::exception &e) {
        std::cerr << "[SERVER] ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
