#!/usr/bin/env python3
"""
Test RTNeural model inference by comparing expected outputs.

This script creates test inputs, runs them through a Python LSTM implementation
using the weights from the JSON file, and outputs expected values that can be
compared against the C++ RTNeural implementation.

Usage:
    python test_model_inference.py <model.json>
"""

import json
import sys
import numpy as np
from pathlib import Path


def flatten_weights(weights: list) -> np.ndarray:
    """Flatten nested weight arrays to 1D numpy array."""
    result = []
    def _flatten(item):
        if isinstance(item, list):
            for sub in item:
                _flatten(sub)
        else:
            result.append(item)
    _flatten(weights)
    return np.array(result, dtype=np.float32)


def sigmoid(x):
    """Numerically stable sigmoid."""
    return np.where(x >= 0,
                    1 / (1 + np.exp(-x)),
                    np.exp(x) / (1 + np.exp(x)))


def tanh(x):
    """Tanh activation."""
    return np.tanh(x)


class LSTM:
    """Simple LSTM implementation matching Keras/RTNeural gate order [i, f, c, o]."""

    def __init__(self, kernel, recurrent, bias, hidden_size=12):
        self.hidden_size = hidden_size

        # Reshape weights
        # Kernel: (1, 4*hidden) - weights from input to gates
        self.W = kernel.reshape(1, 4 * hidden_size)

        # Recurrent: (hidden, 4*hidden) - weights from hidden state to gates
        self.U = recurrent.reshape(hidden_size, 4 * hidden_size)

        # Bias: (4*hidden,)
        self.b = bias

        # Initialize state
        self.reset()

    def reset(self):
        """Reset hidden and cell state."""
        self.h = np.zeros(self.hidden_size, dtype=np.float32)
        self.c = np.zeros(self.hidden_size, dtype=np.float32)

    def forward(self, x):
        """
        Forward pass for single input.

        Gate order: [i, f, c, o] (input, forget, cell candidate, output)
        """
        h = self.hidden_size

        # Compute all gates at once
        # gates = x @ W + h @ U + b
        gates = np.dot(x, self.W) + np.dot(self.h, self.U) + self.b

        # Split into individual gates
        i_gate = sigmoid(gates[0:h])           # Input gate
        f_gate = sigmoid(gates[h:2*h])         # Forget gate
        c_candidate = tanh(gates[2*h:3*h])     # Cell candidate (g)
        o_gate = sigmoid(gates[3*h:4*h])       # Output gate

        # Update cell state
        self.c = f_gate * self.c + i_gate * c_candidate

        # Update hidden state
        self.h = o_gate * tanh(self.c)

        return self.h


class Dense:
    """Simple dense layer."""

    def __init__(self, weights, bias):
        # weights: (12,) -> reshape to (12, 1) for matmul
        self.W = weights.reshape(-1, 1)
        self.b = bias

    def forward(self, x):
        return np.dot(x, self.W).flatten() + self.b


def load_model(json_path: str) -> tuple:
    """Load model weights from JSON."""
    with open(json_path, 'r') as f:
        data = json.load(f)

    layers = data.get('layers', [])
    lstm_weights = layers[0].get('weights', [])
    dense_weights = layers[1].get('weights', [])

    kernel = flatten_weights(lstm_weights[0])
    recurrent = flatten_weights(lstm_weights[1])
    bias = flatten_weights(lstm_weights[2])
    dense_w = flatten_weights(dense_weights[0])
    dense_b = flatten_weights(dense_weights[1])

    return LSTM(kernel, recurrent, bias), Dense(dense_w, dense_b)


def test_model(json_path: str):
    """Run test inference and print expected outputs."""
    print("=" * 70)
    print(f"Model Inference Test: {Path(json_path).name}")
    print("=" * 70)

    lstm, dense = load_model(json_path)

    # Test 1: Impulse response
    print("\n[Test 1: Impulse Response]")
    lstm.reset()
    test_input = [1.0, 0.0, 0.0, 0.0, 0.0]
    print(f"  Input: {test_input}")
    outputs = []
    for x in test_input:
        h = lstm.forward(np.array([x], dtype=np.float32))
        y = dense.forward(h)
        outputs.append(float(y[0]))
    print(f"  Output: {outputs}")
    print(f"  Output[0] (impulse): {outputs[0]:.8f}")

    # Test 2: DC input
    print("\n[Test 2: DC Input (0.5)]")
    lstm.reset()
    test_input = [0.5] * 10
    outputs = []
    for x in test_input:
        h = lstm.forward(np.array([x], dtype=np.float32))
        y = dense.forward(h)
        outputs.append(float(y[0]))
    print(f"  Last 5 outputs: {outputs[-5:]}")

    # Test 3: Sine wave
    print("\n[Test 3: 440Hz Sine Wave (first 20 samples)]")
    lstm.reset()
    sample_rate = 48000.0
    freq = 440.0
    t = np.arange(20) / sample_rate
    sine_input = 0.5 * np.sin(2 * np.pi * freq * t)
    outputs = []
    for x in sine_input:
        h = lstm.forward(np.array([x], dtype=np.float32))
        y = dense.forward(h)
        outputs.append(float(y[0]))
    print(f"  Input[0:5]: {list(sine_input[:5])}")
    print(f"  Output[0:5]: {outputs[:5]}")
    print(f"  Output[15:20]: {outputs[15:]}")

    # Summary for C++ comparison
    print("\n" + "=" * 70)
    print("Expected values for C++ verification:")
    print("=" * 70)

    lstm.reset()
    h = lstm.forward(np.array([1.0], dtype=np.float32))
    y_impulse = dense.forward(h)[0]
    print(f"  Impulse response first sample: {y_impulse:.8f}")

    lstm.reset()
    for _ in range(9):
        lstm.forward(np.array([0.5], dtype=np.float32))
    h = lstm.forward(np.array([0.5], dtype=np.float32))
    y_dc = dense.forward(h)[0]
    print(f"  DC (0.5) after 10 samples: {y_dc:.8f}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python test_model_inference.py <model.json>")
        sys.exit(1)

    json_path = sys.argv[1]
    if not Path(json_path).exists():
        print(f"ERROR: File not found: {json_path}")
        sys.exit(1)

    test_model(json_path)


if __name__ == '__main__':
    main()
