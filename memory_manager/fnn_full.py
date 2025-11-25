#!/usr/bin/env python3
# fnn_full_run.py
# Full end-to-end script for Project 4 FNN experiments (preprocess -> train -> eval -> per-subclass analysis)
#
# Usage example:
# python fnn_full_run.py --train NSL-KDD/Training-a1-a3.csv --test NSL-KDD/Testing-a2-a4.csv --batch 10 --epochs 10 --threshold 0.5

import argparse
import numpy as np
import pandas as pd
import os
import matplotlib.pyplot as plt
from sklearn.compose import ColumnTransformer
from sklearn.preprocessing import OneHotEncoder, StandardScaler
from sklearn.metrics import confusion_matrix, classification_report, accuracy_score
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense
from tensorflow.keras.optimizers import Adam

def load_raw_csv(path):
    """Load raw NSL-KDD style file (no header). Return DataFrame."""
    return pd.read_csv(path, header=None, encoding="ISO-8859-1")

def prepare_xy(train_df, test_df, cat_cols=[1,2,3], label_col=-2):
    """
    Fit one-hot encoder on training categorical columns, then transform train and test.
    Fit scaler on training numeric columns, then transform both.
    Return X_train, X_test (numpy arrays), y_train, y_test (binary 0/1),
    plus raw test labels (attack subclass strings) for per-subclass analysis.
    """
    # Extract raw label strings for test (for per-subclass analysis)
    raw_test_labels = test_df.iloc[:, label_col].astype(str).values

    # X = all columns except last two (features)
    X_train_raw = train_df.iloc[:, :label_col].values
    X_test_raw  = test_df.iloc[:, :label_col].values

    # Labels -> binary (normal = 0, else = 1)
    y_train_raw = train_df.iloc[:, label_col].astype(str).values
    y_test_raw  = test_df.iloc[:, label_col].astype(str).values

    y_train = np.array([0 if s == 'normal' else 1 for s in y_train_raw])
    y_test  = np.array([0 if s == 'normal' else 1 for s in y_test_raw])

    # ColumnTransformer: OneHot for categorical indices, passthrough rest
    ct = ColumnTransformer(
        transformers=[
            ('onehot', OneHotEncoder(handle_unknown='ignore', sparse=False), cat_cols)
        ],
        remainder='passthrough'
    )

    # Fit on training features and transform
    X_train_enc = ct.fit_transform(X_train_raw)
    X_test_enc  = ct.transform(X_test_raw)  # use same encoder so categories align

    # Now scale numeric features (fit on train)
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train_enc)
    X_test_scaled  = scaler.transform(X_test_enc)

    return X_train_scaled, X_test_scaled, y_train, y_test, raw_test_labels, ct, scaler

def build_fnn(input_dim, hidden1=6, hidden2=6, lr=0.001):
    model = Sequential()
    model.add(Dense(units=hidden1, activation='relu', kernel_initializer='glorot_uniform', input_dim=input_dim))
    model.add(Dense(units=hidden2, activation='relu', kernel_initializer='glorot_uniform'))
    model.add(Dense(units=1, activation='sigmoid', kernel_initializer='glorot_uniform'))
    opt = Adam(learning_rate=lr)
    model.compile(optimizer=opt, loss='binary_crossentropy', metrics=['accuracy'])
    return model

def per_subclass_detection(raw_test_labels, y_pred_bin):
    """
    Compute detection rate per subclass label (fraction predicted as attack=1).
    Returns dict: label -> (count, detection_rate)
    """
    subclasses, counts = np.unique(raw_test_labels, return_counts=True)
    results = {}
    for sub in subclasses:
        idx = np.where(raw_test_labels == sub)[0]
        preds = y_pred_bin[idx].astype(int)
        detection_rate = preds.mean() if len(preds) > 0 else 0.0
        results[sub] = (len(idx), detection_rate)
    return results

def print_detection_table(results, top_n=None):
    rows = []
    for k,(count,rate) in results.items():
        rows.append((k,count,rate))
    rows_sorted = sorted(rows, key=lambda x: -x[2])  # sort by detection_rate desc
    if top_n:
        rows_sorted = rows_sorted[:top_n]
    print("\nPer-subclass detection rates (label, count, detection_rate):")
    for label,count,rate in rows_sorted:
        print(f"{label:25s} {count:6d}  {rate*100:6.2f}%")
    return rows_sorted

def main():
    parser = argparse.ArgumentParser(description="FNN end-to-end for NSL-KDD scenario")
    parser.add_argument("--train", required=True, help="Path to training CSV (no header)")
    parser.add_argument("--test", required=True, help="Path to testing CSV (no header)")
    parser.add_argument("--batch", type=int, default=10, help="Batch size")
    parser.add_argument("--epochs", type=int, default=10, help="Epoch count")
    parser.add_argument("--threshold", type=float, default=0.5, help="Probability threshold for attack=1")
    parser.add_argument("--lr", type=float, default=0.001, help="Learning rate for Adam")
    parser.add_argument("--out_prefix", default="fnn_run", help="Prefix for saved plots and outputs")
    args = parser.parse_args()

    # 1) Load raw CSVs (no header)
    print("1) Loading raw CSV files...")
    train_df = load_raw_csv(args.train)
    test_df  = load_raw_csv(args.test)
    print(f"   Training rows: {len(train_df)}, Testing rows: {len(test_df)}")

    # Quick sanity check: ensure 'normal' appears in both
    train_normals = train_df.iloc[:, -2].astype(str).str.lower().eq('normal').sum()
    test_normals  = test_df.iloc[:, -2].astype(str).str.lower().eq('normal').sum()
    print(f"   Normal records: train={train_normals}, test={test_normals}")
    if train_normals == 0 or test_normals == 0:
        print("   WARNING: One of the datasets has 0 'normal' rows. Check your DataExtractor output.")

    # 2) Preprocess: encode categorical + scale numeric
    print("2) Preprocessing: one-hot encode categorical cols [1,2,3] and scale features...")
    X_train, X_test, y_train, y_test, raw_test_labels, ct, scaler = prepare_xy(train_df, test_df)
    print(f"   After transform: X_train.shape={X_train.shape}, X_test.shape={X_test.shape}")

    # 3) Build FNN
    print("3) Building FNN model...")
    model = build_fnn(input_dim=X_train.shape[1], lr=args.lr)
    model.summary()

    # 4) Train
    print(f"4) Training: batch={args.batch}, epochs={args.epochs} ...")
    history = model.fit(X_train, y_train, batch_size=args.batch, epochs=args.epochs, validation_split=0.1, verbose=1)

    # Save training plots
    # Plot accuracy
    plt.figure()
    # keras versions may use 'accuracy' or 'acc'
    acc_key = 'accuracy' if 'accuracy' in history.history else 'acc'
    plt.plot(history.history[acc_key], label='train_acc')
    if 'val_'+acc_key in history.history:
        plt.plot(history.history['val_'+acc_key], label='val_acc')
    plt.title('Model accuracy')
    plt.xlabel('epoch'); plt.ylabel('accuracy'); plt.legend()
    acc_plot = f"{args.out_prefix}_accuracy.png"
    plt.savefig(acc_plot)
    print(f"   Saved accuracy plot to {acc_plot}")

    # Plot loss
    plt.figure()
    plt.plot(history.history['loss'], label='train_loss')
    if 'val_loss' in history.history:
        plt.plot(history.history['val_loss'], label='val_loss')
    plt.title('Model loss')
    plt.xlabel('epoch'); plt.ylabel('loss'); plt.legend()
    loss_plot = f"{args.out_prefix}_loss.png"
    plt.savefig(loss_plot)
    print(f"   Saved loss plot to {loss_plot}")

    # 5) Evaluate on test set
    print("5) Evaluating on test set...")
    test_loss, test_acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"   Test Loss: {test_loss:.4f}, Test Accuracy: {test_acc:.4f}")

    # Predictions and thresholding
    y_pred_prob = model.predict(X_test).reshape(-1)
    y_pred_bin = (y_pred_prob >= args.threshold).astype(int)

    print("\n6) Confusion Matrix and classification report:")
    cm = confusion_matrix(y_test, y_pred_bin)
    print("Confusion matrix [TN FP; FN TP]:")
    print(cm)
    print("\nClassification report:")
    print(classification_report(y_test, y_pred_bin, digits=4))
    print("Overall accuracy (sklearn):", accuracy_score(y_test, y_pred_bin))

    # 7) Per-subclass detection rates
    print("\n7) Per-subclass detection analysis (on raw test labels):")
    results = per_subclass_detection(raw_test_labels, y_pred_bin)
    # Print sorted table (descending detection rate)
    _ = print_detection_table(results, top_n=None)

    # Save per-subclass results to CSV
    rows = [(label,count,rate) for label,(count,rate) in results.items()]
    df_rows = pd.DataFrame(rows, columns=['label','count','detection_rate'])
    df_rows.sort_values(by='detection_rate', ascending=False, inplace=True)
    out_csv = f"{args.out_prefix}_per_subclass.csv"
    df_rows.to_csv(out_csv, index=False)
    print(f"\n   Saved per-subclass detection rates to {out_csv}")

    print("\nDone. Review the confusion matrix, classification report, and per-subclass file for insights.")

if __name__ == "__main__":
    main()
