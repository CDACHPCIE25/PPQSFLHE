#!/usr/bin/env python3
"""
analyze_comm_metrics_refactored.py

Reads:
  - comm_metrics.csv (client-side instrumentation)
  - server_comm_metrics.csv (server-side instrumentation)

Produces:
  - human-readable summary on stdout
  - cross-check report (mismatches printed)
  - plots in ./plots/: stacked_bytes_by_type.png, round_payloads.png, latency_hist.png

Place this script in your metrics folder (or pass metrics_dir) and run it.
"""

import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
import warnings

# ----------------------
# Configuration
# ----------------------
METRICS_DIR = "."  # adjust if running from different dir
CLIENT_FILE = "comm_metrics.csv"
SERVER_FILE = "server_comm_metrics.csv"
PLOTS_DIR = os.path.join(METRICS_DIR, "plots")
os.makedirs(PLOTS_DIR, exist_ok=True)

# ----------------------
# Helpers
# ----------------------
def safe_read_csv(path):
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    df = pd.read_csv(path, dtype=str)
    # ensure all expected columns exist (add if missing)
    expected = ["timestamp","role","method","endpoint","client_id","type","file",
                "payload_size","bytes_sent","bytes_received","latency_ms","http_code"]
    for c in expected:
        if c not in df.columns:
            df[c] = np.nan
    return df[expected]

def to_int_series(s):
    return pd.to_numeric(s, errors="coerce").fillna(0).astype(np.int64)

def friendly_bytes(n):
    # n in bytes -> human readable
    if n is None:
        return "0 B"
    n = float(n)
    for unit in ["B","KB","MB","GB","TB"]:
        if n < 1024.0:
            return f"{n:0.2f} {unit}"
        n /= 1024.0
    return f"{n:0.2f} PB"

# ----------------------
# Load + Clean
# ----------------------
def load_and_clean(metrics_dir=METRICS_DIR):
    client_path = os.path.join(metrics_dir, CLIENT_FILE)
    server_path = os.path.join(metrics_dir, SERVER_FILE)

    df_client = safe_read_csv(client_path)
    df_server = safe_read_csv(server_path)

    # unify timestamp format; tolerant parsing
    for df in (df_client, df_server):
        # try multiple timestamp formats; common format in your examples: "18-09-2025 12:03"
        df['timestamp'] = pd.to_datetime(df['timestamp'], format="%d-%m-%Y %H:%M", errors='coerce')
        if df['timestamp'].isna().all():
            # fallback to pandas auto parser
            df['timestamp'] = pd.to_datetime(df['timestamp'], errors='coerce')

        # normalize endpoint and file strings
        df['endpoint'] = df['endpoint'].fillna("").astype(str)
        df['file'] = df['file'].fillna("").astype(str)

        # client id: normalize empty
        df['client_id'] = df['client_id'].replace({np.nan: ""}).astype(str)

        # type: normalize common values
        df['type'] = df['type'].fillna("").astype(str)

        # numeric columns
        df['payload_size'] = to_int_series(df['payload_size'])
        df['bytes_sent'] = to_int_series(df['bytes_sent'])
        df['bytes_received'] = to_int_series(df['bytes_received'])
        df['latency_ms'] = to_int_series(df['latency_ms'])
        df['http_code'] = to_int_series(df['http_code'])

    # attempt to infer type if empty (simple heuristics)
    def infer_type_row(row):
        if row['type']:
            return row['type']
        f = row['file'].lower()
        e = row['endpoint'].lower()
        if 'cc.json' in f or 'cc.json' in e:
            return 'config'
        if 'public' in f or 'public' in e:
            return 'pubkey'
        if 'rekey' in f or 're-key' in f or 'rekey' in e:
            return 'rekey'
        if 'weight' in f or 'aggreg' in f or 'domainchange' in f or 'domain_change' in f:
            return 'weights'
        return 'unknown'
    df_client['type'] = df_client.apply(infer_type_row, axis=1)
    df_server['type'] = df_server.apply(infer_type_row, axis=1)

    return df_client, df_server

# ----------------------
# Cross-check client <-> server
# ----------------------
def cross_check(client_df, server_df, time_window_seconds=60):
    """Return list of mismatch messages. Match on endpoint+basename(file) within window."""
    server_df = server_df.copy()
    client_df = client_df.copy()

    # normalize to basename for file comparisons
    server_df['file_base'] = server_df['file'].apply(lambda f: os.path.basename(f) if f else "")
    client_df['file_base'] = client_df['file'].apply(lambda f: os.path.basename(f) if f else "")

    server_df['key'] = server_df['endpoint'].str.strip() + "||" + server_df['file_base']
    client_df['key'] = client_df['endpoint'].str.strip() + "||" + client_df['file_base']

    mismatches = []
    matched_pairs = []

    # index server by key
    server_index = {}
    for i, r in server_df.iterrows():
        server_index.setdefault(r['key'], []).append((i, r))

    for i, crow in client_df.iterrows():
        key = crow['key']
        if key not in server_index:
            mismatches.append(
                f"No server entry for client row [{crow['method']} {crow['endpoint']} file={crow['file_base']}]"
            )
            continue

        # find close-in-time match
        candidates = server_index[key]
        best = None
        for j, srow in candidates:
            if pd.isna(crow['timestamp']) or pd.isna(srow['timestamp']):
                best = (j, srow)  # accept if no timestamp
                break
            dt = abs((srow['timestamp'] - crow['timestamp']).total_seconds())
            if dt <= time_window_seconds:
                best = (j, srow)
                break

        if best is None:
            mismatches.append(
                f"No time-close server match for client [{crow['method']} {crow['endpoint']} file={crow['file_base']}]"
            )
            continue

        _, srow = best
        matched_pairs.append((crow, srow))

        # Compare sizes
        if crow['method'].upper() == 'POST':
            if abs(int(crow['payload_size']) - int(srow['bytes_received'])) > max(1, int(0.01 * max(1, crow['payload_size']))):
                mismatches.append(
                    f"POST size mismatch for {crow['file_base']}: client payload={crow['payload_size']} vs server received={srow['bytes_received']}"
                )
        elif crow['method'].upper() == 'GET':
            if srow['payload_size'] and abs(int(srow['payload_size']) - int(crow['bytes_received'])) > max(1, int(0.01 * max(1, srow['payload_size']))):
                mismatches.append(
                    f"GET size mismatch for {crow['file_base']}: server payload={srow['payload_size']} vs client received={crow['bytes_received']}"
                )

    return mismatches

# ----------------------
# Metrics summaries + plots
# ----------------------
def summarize_and_plot(client_df, server_df):
    # Only use client-side for overhead calculation: bytes_sent - payload_size
    client = client_df.copy()
    server = server_df.copy()

    client['method'] = client['method'].str.upper()
    server['method'] = server['method'].str.upper()

    # Compute overhead only for client POSTs (network bytes sent vs payload)
    client_post = client[client['method'] == 'POST'].copy()
    client_post['overhead_bytes'] = client_post['bytes_sent'] - client_post['payload_size']
    # sanitize negative overhead (warn)
    neg_overhead_count = (client_post['overhead_bytes'] < 0).sum()
    if neg_overhead_count > 0:
        warnings.warn(f"Found {neg_overhead_count} client POST rows with negative overhead (bytes_sent < payload_size). Check instrumentation.")

    # Human readable summary
    print("\n================ CLIENT SUMMARY ================\n")
    total_client_payload = client['payload_size'].sum()
    total_client_sent = client['bytes_sent'].sum()
    print(f"Total client payload (all operations): {friendly_bytes(total_client_payload)}")
    print(f"Total client bytes_sent (approx):       {friendly_bytes(total_client_sent)}")
    print(f"Total client uploads (POST payload sum): {friendly_bytes(client_post['payload_size'].sum())}")
    print(f"Avg client latency (ms) [all]:           {int(client['latency_ms'].mean()):,} ms")
    print(f"Avg client latency (ms) [POST only]:     {int(client_post['latency_ms'].mean() or 0):,} ms")
    print(f"Average overhead (bytes) [POST only]:    {int(client_post['overhead_bytes'].mean() or 0):,} bytes")
    print(f"Average overhead % [POST only]:          {float((client_post['overhead_bytes'].sum()) / max(1, client_post['payload_size'].sum()) * 100 or 0):.4f}%")
    if neg_overhead_count:
        print(f"WARNING: {neg_overhead_count} client POST rows have negative overhead (instrumentation mismatch).")

    print("\nBreakdown (client) by type:")
    by_type = client.groupby('type').agg({'payload_size':'sum','bytes_sent':'sum','latency_ms':'mean'})
    for t, row in by_type.iterrows():
        print(f" - {t:8}: payload={friendly_bytes(row['payload_size'])}, sent={friendly_bytes(row['bytes_sent'])}, avg_latency={int(row['latency_ms'])} ms")

    # Server summary
    print("\n================ SERVER SUMMARY ================\n")
    total_server_received = server['bytes_received'].sum()
    print(f"Total server bytes_received (all operations): {friendly_bytes(total_server_received)}")
    print(f"Total server payload_size (GET served):       {friendly_bytes(server['payload_size'].sum())}")
    print(f"Avg server latency (ms):                      {int(server['latency_ms'].mean() or 0)} ms")

    print("\nBreakdown (server) by type:")
    by_type_s = server.groupby('type').agg({'payload_size':'sum','bytes_received':'sum','latency_ms':'mean'})
    for t, row in by_type_s.iterrows():
        print(f" - {t:8}: payload={friendly_bytes(row['payload_size'])}, received={friendly_bytes(row['bytes_received'])}, avg_latency={int(row['latency_ms'])} ms")

    # Per-round aggregation
    # Use 2-minute windows, but use '2min' to avoid future warning
    combined = pd.concat([client.assign(role='client'), server.assign(role='server')], ignore_index=True)
    combined['round_time'] = combined['timestamp'].dt.floor('2min')
    round_summary = combined.groupby(['round_time','role']).agg({
        'payload_size':'sum',
        'bytes_sent':'sum',
        'bytes_received':'sum'
    }).reset_index()
    # print a human-friendly round summary
    print("\n================ PER-ROUND SUMMARY ================\n")
    for rt in sorted(round_summary['round_time'].unique()):
        rows = round_summary[round_summary['round_time']==rt]
        print(f"Round starting {rt}:")
        for _, r in rows.iterrows():
            print(f"  {r['role']:6} payload={friendly_bytes(r['payload_size'])}, sent={friendly_bytes(r['bytes_sent'])}, recv={friendly_bytes(r['bytes_received'])}")
        print("")

    # Cross-check
    mismatches = cross_check(client, server)
    print("\n================ CROSS-CHECK ================\n")
    if not mismatches:
        print("OK: All matched between client and server (within tolerance).")
    else:
        print("Mismatches found:")
        for m in mismatches:
            print(" -", m)

    # -------------------
    # Plots
    # -------------------
    print("\nGenerating plots in", PLOTS_DIR)

    # Plot A: Stacked bar of payload bytes by type and role
    try:
        agg = combined.groupby(['role','type'])['payload_size'].sum().unstack(fill_value=0)
        # convert to MB
        agg_mb = agg / 1024.0 / 1024.0
        ax = agg_mb.plot(kind='bar', stacked=True, figsize=(9,6))
        plt.title("Payload (MB) by Type and Role")
        plt.xlabel("Role")
        plt.ylabel("Payload (MB)")
        plt.tight_layout()
        out = os.path.join(PLOTS_DIR, "stacked_bytes_by_type.png")
        plt.savefig(out)
        plt.close()
    except Exception as e:
        print("Plot A failed:", e)

    # Plot B: Line chart of per-round payload sizes (client uploads vs downloads)
    try:
        rs = round_summary.copy()
        rs['payload_MB'] = rs['payload_size'] / 1024.0 / 1024.0
        pivot = rs.pivot(index='round_time', columns='role', values='payload_MB').fillna(0)
        pivot.plot(kind='line', marker='o', figsize=(9,5))
        plt.title("Per-round payload (MB) by role (2-minute rounds)")
        plt.xlabel("Round start time")
        plt.ylabel("Payload MB")
        plt.grid(axis='y', alpha=0.3)
        plt.tight_layout()
        out = os.path.join(PLOTS_DIR, "round_payloads.png")
        plt.savefig(out)
        plt.close()
    except Exception as e:
        print("Plot B failed:", e)

    # Plot C: Latency histogram (client vs server)
    try:
        plt.figure(figsize=(9,5))
        c_lat = client['latency_ms'].replace(0, np.nan).dropna()
        s_lat = server['latency_ms'].replace(0, np.nan).dropna()
        bins = np.histogram_bin_edges(pd.concat([c_lat, s_lat], ignore_index=True).dropna(), bins='auto') if (not c_lat.empty or not s_lat.empty) else [0,1]
        plt.hist(c_lat, bins=bins, alpha=0.6, label='client', density=False)
        plt.hist(s_lat, bins=bins, alpha=0.6, label='server', density=False)
        plt.legend()
        plt.title("Latency distribution (ms)")
        plt.xlabel("Latency (ms)")
        plt.ylabel("Count")
        plt.tight_layout()
        out = os.path.join(PLOTS_DIR, "latency_hist.png")
        plt.savefig(out)
        plt.close()
    except Exception as e:
        print("Plot C failed:", e)

    print("Plots saved.")

# ----------------------
# Main
# ----------------------
def main():
    try:
        df_client, df_server = load_and_clean()
    except FileNotFoundError as e:
        print("ERROR: metrics file not found:", e)
        return

    summarize_and_plot(df_client, df_server)

if __name__ == "__main__":
    main()
