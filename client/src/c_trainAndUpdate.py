import os, sys, json
import pandas as pd
import numpy as np
from datetime import datetime
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score
import tensorflow as tf
from tensorflow.keras.models import Sequential, load_model
from tensorflow.keras.layers import GRU, Dense, Dropout, BatchNormalization
from tensorflow.keras.callbacks import EarlyStopping, ModelCheckpoint
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.regularizers import l2
from tensorflow.keras import backend as K

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"  # 0=all logs, 1=filter INFO, 2=filter WARN, 3=filter ERROR
tf.get_logger().setLevel("ERROR") # kills the CUDA/cuDNN spam.

# ----------------------
# Helpers
# ----------------------

def load_config(config_path):
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    with open(config_path, "r") as f:
        cfg = json.load(f)
    for key in [
        "INPUT_WEIGHTS_PATH",
        "OUTPUT_DECRYPTED_WEIGHTS_PATH",
        "data_file", "output_file", "log_dir", "model_file"
    ]:
        if key in cfg["CLIENT"]:
            cfg["CLIENT"][key] = os.path.join(base_dir, cfg["CLIENT"][key])
    return cfg

def prepare_sequences(df, lookback, feature_names, target_name, fs, ts):
    features = fs.transform(df[feature_names].values)
    targets = ts.transform(df[[target_name]].values)
    seqs, targs = [], []
    for i in range(lookback, len(df)):
        seq = np.concatenate([features[i-lookback:i], targets[i-lookback:i]], axis=1)
        seqs.append(seq)
        targs.append(targets[i,0])
    return np.array(seqs), np.array(targs)

def create_model(lookback, n_features):
    model = Sequential([
        GRU(32, input_shape=(lookback, n_features), return_sequences=False, kernel_regularizer=l2(0.01)),
        Dropout(0.2),
        #BatchNormalization(),
        Dense(1)
    ])
    model.compile(optimizer=Adam(0.001), loss="mse")
    return model

def calc_metrics(y_true, y_pred, y_mean):
    mae = float(mean_absolute_error(y_true, y_pred))
    rmse = float(np.sqrt(mean_squared_error(y_true, y_pred)))
    r2 = float(r2_score(y_true, y_pred))
    pmae = float(mae / y_mean * 100) if y_mean != 0 else 0.0
    return {"MAE": mae, "RMSE": rmse, "R2": r2, "PMAE": pmae}

def reconstruct_model_from_json(json_path, lookback, n_features):
    """ Rebuild Keras model and load weights from decrypted JSON """
    with open(json_path, "r") as f:
        data = json.load(f)

    weights_summary = data["weights_summary"]
    layer_weights = []
    for entry in weights_summary:
        shape = tuple(entry["shape"])
        values = np.array(entry["values"], dtype=np.float32).reshape(shape)
        layer_weights.append(values)

    model = create_model(lookback, n_features)
    model.set_weights(layer_weights)
    return model

# ----------------------
# Main Training + Update
# ----------------------

def main(config_path):
    cfg = load_config(config_path)["CLIENT"]
    client_id = cfg["client_id"]
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    os.makedirs(cfg["log_dir"], exist_ok=True)

    # ----------------------------
    # Load data
    # ----------------------------
    df = pd.read_csv(cfg["data_file"])

    # Ensure Timestamp column is datetime
    if "Timestamp" not in df.columns:
        raise ValueError(f"[{client_id}] Expected column 'Timestamp' not found. Got {list(df.columns)}")
    
    df["Timestamp"] = pd.to_datetime(df["Timestamp"], errors="coerce")
    
    if df["Timestamp"].isnull().any():
        print(f"[{client_id}] Warning: Some rows had invalid timestamps and were set to NaT")
        
    df["DayOfYear"] = df["Timestamp"].dt.dayofyear
    df["Month"] = df["Timestamp"].dt.month
    df["DayOfWeek"] = df["Timestamp"].dt.dayofweek
    df["WeekOfYear"] = df["Timestamp"].dt.isocalendar().week
    df["AcademicMonth"] = df["Month"].apply(lambda x: 1 if x in [1,2,3,4,5,8,9,10,11] else 0)
    df["HourOfDay"] = df["Timestamp"].dt.hour

    feature_names = ["DayOfYear","Month","DayOfWeek","WeekOfYear","AcademicMonth","HourOfDay"]
    target = "Data"

    train = df[df["Timestamp"] <= cfg["train_end_date"]]
    test  = df[df["Timestamp"] >= cfg["test_start_date"]]

    fs, tscl = StandardScaler(), StandardScaler()
    fs.fit(train[feature_names].values)
    tscl.fit(train[[target]].values)

    X_train, y_train = prepare_sequences(train, cfg["lookback"], feature_names, target, fs, tscl)
    X_val, y_val = X_train[-int(0.1*len(X_train)):], y_train[-int(0.1*len(y_train)):]
    X_train, y_train = X_train[:-int(0.1*len(X_train))], y_train[:-int(0.1*len(y_train))]

    # ----------------------------
    # Load global model if available
    # ----------------------------
    if os.path.exists(cfg["OUTPUT_DECRYPTED_WEIGHTS_PATH"]):
        print(f"[{client_id}] Loading global model from {cfg['OUTPUT_DECRYPTED_WEIGHTS_PATH']}")
        model = reconstruct_model_from_json(cfg["OUTPUT_DECRYPTED_WEIGHTS_PATH"], cfg["lookback"], cfg["n_features"]+1)
    else:
        print(f"[{client_id}] No global model found, creating fresh model")
        model = create_model(cfg["lookback"], cfg["n_features"]+1)

    # ----------------------------
    # Local Training
    # ----------------------------
    ckpt = os.path.join(cfg["log_dir"], f"{client_id}_best_{ts}.keras")
    history = model.fit(
        X_train, y_train,
        epochs=25, batch_size=16,
        validation_data=(X_val, y_val),
        callbacks=[
            EarlyStopping(monitor="val_loss", patience=4),
            ModelCheckpoint(ckpt, save_best_only=True, monitor="val_loss")
        ],
        verbose=1
    )
    model = load_model(ckpt)

    # Save locally for next round
    model.save(cfg["model_file"])
    print(f"[{client_id}] Model saved -> {cfg['model_file']}")

    # ----------------------------
    # Export weights to JSON for encryption
    # ----------------------------
    #weights = model.get_weights()
    weights_summary = []
    
    for w in model.weights:
        arr = w.numpy()
        weights_summary.append({
          "layer": w.name.replace(":0", ""),                  
          "shape": list(arr.shape),
          "mean": float(np.mean(arr)),
          "std_dev": float(np.std(arr)),
          "values": arr.flatten().tolist()
    })

    with open(cfg["INPUT_WEIGHTS_PATH"], "w") as f:
        json.dump({"weights_summary": weights_summary}, f, indent=2)

    print(f"[{client_id}] Weights exported -> {cfg['INPUT_WEIGHTS_PATH']}")

    # ----------------------------
    # Evaluation
    # ----------------------------
    def inv(pred): return tscl.inverse_transform(pred.reshape(-1,1)).flatten()
    train_pred, val_pred = inv(model.predict(X_train,verbose=0).flatten()), inv(model.predict(X_val,verbose=0).flatten())
    y_train_true, y_val_true = inv(y_train), inv(y_val)
    mtrain, mval = calc_metrics(y_train_true, train_pred, np.mean(y_train_true)), calc_metrics(y_val_true, val_pred, np.mean(y_val_true))
    print(f"[{client_id}] Train Metrics:", mtrain)
    print(f"[{client_id}] Val Metrics:", mval)

    K.clear_session()

if __name__ == "__main__":
    if len(sys.argv)<2:
        print("Usage: python c_train_update.py <config.json>")
        sys.exit(1)
    main(sys.argv[1])
