import os
import json
import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score
from tensorflow.keras.models import load_model
from tensorflow.keras import backend as K

# -----------------------------
# Config
# -----------------------------
CLIENT_CONFIG = "/home/hpcie/Desktop/PPFL/client/config/client_2/c_config.json"
LOOKBACK = 72
N_FEATURES = 6  # DayOfYear, Month, DayOfWeek, WeekOfYear, AcademicMonth, HourOfDay
TARGET = "Data"
LOG_DIR = "logs"
RESULTS_DIR = "results"
os.makedirs(os.path.join(os.path.dirname(__file__), RESULTS_DIR), exist_ok=True)

# -----------------------------
# Helpers
# -----------------------------
def load_config(path):
    with open(path, "r") as f:
        cfg = json.load(f)

    client_cfg = cfg["CLIENT"]
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../.."))

    for key, val in client_cfg.items():
        if isinstance(val, str) and (key.endswith("_file") or key.endswith("_path") or "PATH" in key.upper()):
            if not os.path.isabs(val):
                if val.startswith("client/"):
                    client_cfg[key] = os.path.join(repo_root, val)
                else:
                    client_cfg[key] = os.path.join(repo_root, "client", val)
    return client_cfg

def prepare_sequences(df, lookback, feature_names, target_name, fs, ts):
    features = fs.transform(df[feature_names].values)
    targets_scaled = ts.transform(df[[target_name]].values)
    sequences, targets = [], []
    for i in range(lookback, len(df)):
        seq = np.concatenate([features[i-lookback:i], targets_scaled[i-lookback:i]], axis=1)
        sequences.append(seq)
        targets.append(targets_scaled[i, 0])
    return np.array(sequences), np.array(targets)

def calculate_metrics(y_true, y_pred, y_mean):
    mae = mean_absolute_error(y_true, y_pred)
    rmse = np.sqrt(mean_squared_error(y_true, y_pred))
    r2 = r2_score(y_true, y_pred)
    mape = np.mean(np.abs((y_true - y_pred) / np.where(y_true != 0, y_true, 1))) * 100
    pmae = (mae / y_mean) * 100 if y_mean != 0 else 0
    return {"MAE": mae, "RMSE": rmse, "R2": r2, "MAPE": mape, "PMAE": pmae}


# -----------------------------
# Main Evaluation
# -----------------------------
def main():
    cfg = load_config(CLIENT_CONFIG)
    client_id = cfg["client_id"]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    # Load dataset
    df = pd.read_csv(cfg["data_file"])
    if "Timestamp" in df.columns:
        df["Timestamp"] = pd.to_datetime(df["Timestamp"], errors="coerce")
    else:
        raise KeyError("Expected column 'Timestamp' not found in CSV")
    
    # Feature engineering
    df["DayOfYear"] = df["Timestamp"].dt.dayofyear
    df["Month"] = df["Timestamp"].dt.month
    df["DayOfWeek"] = df["Timestamp"].dt.dayofweek
    df["WeekOfYear"] = df["Timestamp"].dt.isocalendar().week
    df["AcademicMonth"] = df["Month"].apply(lambda x: 1 if x in [1, 2, 3, 4, 5, 8, 9, 10, 11] else 0)
    df["HourOfDay"] = df["Timestamp"].dt.hour

    # Train/test split
    test_df = df[df["Timestamp"] >= cfg["test_start_date"]]
    train_df = df[df["Timestamp"] <= cfg["train_end_date"]]
    feature_names = ["DayOfYear", "Month", "DayOfWeek", "WeekOfYear", "AcademicMonth", "HourOfDay"]

    # Fit scalers
    fs, ts = StandardScaler(), StandardScaler()
    fs.fit(train_df[feature_names].values)
    ts.fit(train_df[[TARGET]].values)

    # Prepare test sequences
    X_test, y_test = prepare_sequences(test_df, LOOKBACK, feature_names, TARGET, fs, ts)
    y_test_true = ts.inverse_transform(y_test.reshape(-1, 1)).flatten()

    # Load models
    log_dir_path = os.path.join(os.path.dirname(__file__), LOG_DIR)
    round_models = sorted(glob.glob(os.path.join(log_dir_path, f"{client_id}_best_*.keras")))
    if not round_models:
        print(f"No round models found in {log_dir_path}")
        return

    # Collect metrics
    metrics_per_round = []

    for round_idx, model_path in enumerate(round_models, start=1):
        model = load_model(model_path)
        preds_scaled = model.predict(X_test, verbose=0).flatten()
        preds = ts.inverse_transform(preds_scaled.reshape(-1, 1)).flatten()

        # Save round predictions
        results_dir_path = os.path.join(os.path.dirname(__file__), RESULTS_DIR)
        out_csv = os.path.join(results_dir_path, f"{client_id}_round{round_idx}_predictions_{timestamp}.csv")
        pred_df = pd.DataFrame({
            "Timestamp": test_df["Timestamp"][LOOKBACK:].values,
            "True": y_test_true,
            "Pred": preds
        })
        pred_df.to_csv(out_csv, index=False)
        print(f"Round {round_idx} predictions saved to: {out_csv}")

        # Calculate and save metrics
        metrics = calculate_metrics(y_test_true, preds, np.mean(y_test_true))
        metrics["Round"] = round_idx
        metrics_per_round.append(metrics)
        print(f"Round {round_idx} Metrics: {metrics}")

    # Save metrics CSV
    metrics_df = pd.DataFrame(metrics_per_round)
    metrics_csv = os.path.join(results_dir_path, f"{client_id}_metrics_rounds_{timestamp}.csv")
    metrics_df.to_csv(metrics_csv, index=False)
    print(f"Metrics saved to: {metrics_csv}")

    results_dir_path = os.path.join(os.path.dirname(__file__), RESULTS_DIR)
    
    # Look for existing metrics CSV
    metrics_files = sorted(glob.glob(os.path.join(results_dir_path, f"{client_id}_metrics_rounds_*.csv")))
    
    if metrics_files:
        print("Found metrics CSV, using last one for plotting...")
        metrics_csv = metrics_files[-1]
        metrics_df = pd.read_csv(metrics_csv)
    else:
        print("No metrics CSV found. Please run round evaluations first.")
        return
    
    # --- Plot metrics cleanly ---
    plt.figure(figsize=(10, 6))
    for metric in ["MAE", "RMSE", "R2", "MAPE", "PMAE"]:
        plt.plot(metrics_df["Round"], metrics_df[metric], marker="o", label=metric)

    plt.xlabel("Round")
    plt.ylabel("Metric Value")
    plt.title(f"{client_id} Round-wise Metrics")
    plt.legend()
    plt.grid(True)
    plt.xticks(metrics_df["Round"])  # Force integer ticks
    plot_path = os.path.join(results_dir_path, f"{client_id}_round_metrics_{timestamp}.png")
    plt.savefig(plot_path, dpi=300)
    plt.close()
    print(f"Metrics plot saved to: {plot_path}")
    
    # --- Plot predictions per round ---
    pred_files = sorted(glob.glob(os.path.join(results_dir_path, f"{client_id}_round*_predictions_*.csv")))
    
    if not pred_files:
        print("No prediction CSVs found. Skipping prediction plots.")
    else:
        for pred_file in pred_files:
            round_num = int(os.path.basename(pred_file).split("_round")[1].split("_")[0])  # extract round number
            pred_df = pd.read_csv(pred_file)
            pred_df["Timestamp"] = pd.to_datetime(pred_df["Timestamp"])

            plt.figure(figsize=(12, 6))
            plt.plot(pred_df["Timestamp"], pred_df["True"], label="True", linewidth=2)
            plt.plot(pred_df["Timestamp"], pred_df["Pred"], label="Pred", linewidth=2, linestyle="--")
            
            # ? Format x-axis to daily ticks (avoid overcrowding)
            ax = plt.gca()
            ax.xaxis.set_major_locator(mdates.DayLocator(interval=2))  # every 2nd day
            ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
            plt.gcf().autofmt_xdate()  # rotate dates
            
            plt.xlabel("Time")
            plt.ylabel("Value")
            plt.title(f"{client_id} Round {round_num} Predictions (Day-wise)")
            plt.legend()
            plt.grid(True)

            pred_plot_path = os.path.join(
                results_dir_path,
                f"{client_id}_round{round_num}_predictions_{timestamp}.png"
            )
            plt.savefig(pred_plot_path, dpi=300)
            plt.close()
            print(f"Prediction plot saved to: {pred_plot_path}")

    K.clear_session()

if __name__ == "__main__":
    main()
