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
CLIENT_CONFIG = "../../../config/client_1/c_config.json"
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

    # Prepare train/test sequences
    X_train, y_train = prepare_sequences(train_df, LOOKBACK, feature_names, TARGET, fs, ts)
    y_train_true = ts.inverse_transform(y_train.reshape(-1, 1)).flatten()
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

        # Train predictions
        preds_train_scaled = model.predict(X_train, verbose=0).flatten()
        preds_train = ts.inverse_transform(preds_train_scaled.reshape(-1, 1)).flatten()
        train_metrics = calculate_metrics(y_train_true, preds_train, np.mean(y_train_true))

        # Test predictions
        preds_test_scaled = model.predict(X_test, verbose=0).flatten()
        preds_test = ts.inverse_transform(preds_test_scaled.reshape(-1, 1)).flatten()
        test_metrics = calculate_metrics(y_test_true, preds_test, np.mean(y_test_true))

        # Save round predictions (test side only, like before)
        results_dir_path = os.path.join(os.path.dirname(__file__), RESULTS_DIR)
        out_csv = os.path.join(results_dir_path, f"{client_id}_round{round_idx}_predictions_{timestamp}.csv")
        pred_df = pd.DataFrame({
            "Timestamp": test_df["Timestamp"][LOOKBACK:].values,
            "True": y_test_true,
            "Pred": preds_test
        })
        pred_df.to_csv(out_csv, index=False)
        print(f"Round {round_idx} predictions saved to: {out_csv}")

        # Merge metrics
        metrics = {"Round": round_idx}
        metrics.update({f"train_{k}": v for k, v in train_metrics.items()})
        metrics.update({f"test_{k}": v for k, v in test_metrics.items()})
        metrics_per_round.append(metrics)
        print(f"Round {round_idx} Train Metrics: {train_metrics}")
        print(f"Round {round_idx} Test Metrics: {test_metrics}")

    # Save metrics CSV
    metrics_df = pd.DataFrame(metrics_per_round)
    results_dir_path = os.path.join(os.path.dirname(__file__), RESULTS_DIR)
    metrics_csv = os.path.join(results_dir_path, f"{client_id}_metrics_rounds_{timestamp}.csv")
    metrics_df.to_csv(metrics_csv, index=False)
    print(f"Metrics saved to: {metrics_csv}")

    # --- Plot metrics cleanly (train vs test) ---
    plt.figure(figsize=(10, 6))
    for metric in ["MAE", "RMSE", "R2", "MAPE", "PMAE"]:
        plt.plot(metrics_df["Round"], metrics_df[f"train_{metric}"], marker="o", label=f"Train {metric}")
        plt.plot(metrics_df["Round"], metrics_df[f"test_{metric}"], marker="x", linestyle="--", label=f"Test {metric}")

    plt.xlabel("Round")
    plt.ylabel("Metric Value")
    plt.title(f"{client_id} Round-wise Train vs Test Metrics")
    plt.grid(True)
    plt.xticks(metrics_df["Round"])

    # make y-axis consistent across clients
    ymin = metrics_df[[c for c in metrics_df.columns if c != "Round"]].min().min()
    ymax = metrics_df[[c for c in metrics_df.columns if c != "Round"]].max().max()
    plt.ylim(ymin * 1.1, ymax * 1.1)

    # put legend outside
    plt.legend(bbox_to_anchor=(1.05, 1), loc="upper left", borderaxespad=0.)
    plt.tight_layout()

    plot_path = os.path.join(results_dir_path, f"{client_id}_round_metrics_{timestamp}.png")
    plt.savefig(plot_path, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Metrics plot saved to: {plot_path}")

    # --- Plot predictions per round (test only) ---
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
            
            ax = plt.gca()
            ax.xaxis.set_major_locator(mdates.DayLocator(interval=2))  # every 2nd day
            ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
            plt.gcf().autofmt_xdate()

            plt.xlabel("Time")
            plt.ylabel("Value")
            plt.title(f"{client_id} Round {round_num} Predictions (Day-wise)")
            plt.legend(bbox_to_anchor=(1.05, 1), loc="upper left", borderaxespad=0.)
            plt.tight_layout()
            plt.grid(True)

            pred_plot_path = os.path.join(results_dir_path, f"{client_id}_round{round_num}_predictions_{timestamp}.png")
            plt.savefig(pred_plot_path, dpi=300)
            plt.close()
            print(f"Prediction plot saved to: {pred_plot_path}")

    K.clear_session()


if __name__ == "__main__":
    main()