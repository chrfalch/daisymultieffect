#!/usr/bin/env python3
"""
Inspect AIDA-X neural amp model JSON files to understand their structure.

Prints layer types, weight shapes, gate order info, and sample values to help
verify the conversion to C++ headers is correct.

Usage:
    python inspect_model_json.py <model.json>
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


def get_shape(weights: list) -> tuple:
    """Get shape of nested list structure."""
    if not isinstance(weights, list) or len(weights) == 0:
        return ()
    if not isinstance(weights[0], list):
        return (len(weights),)
    return (len(weights),) + get_shape(weights[0])


def inspect_model(json_path: str) -> None:
    """Inspect and print detailed info about an AIDA-X model."""
    with open(json_path, 'r') as f:
        data = json.load(f)

    print("=" * 70)
    print(f"AIDA-X Model Inspector: {Path(json_path).name}")
    print("=" * 70)

    # Metadata
    print("\n[METADATA]")
    metadata = data.get('metadata', {})
    if metadata:
        for key, value in metadata.items():
            print(f"  {key}: {value}")
    else:
        print("  (no metadata found)")

    # Input shape
    in_shape = data.get('in_shape', [])
    print(f"\n[INPUT SHAPE]")
    print(f"  in_shape: {in_shape}")

    # Layers
    layers = data.get('layers', [])
    print(f"\n[LAYERS] ({len(layers)} total)")

    for i, layer in enumerate(layers):
        layer_type = layer.get('type', 'unknown')
        activation = layer.get('activation', '')
        shape = layer.get('shape', [])
        weights = layer.get('weights', [])

        print(f"\n  Layer {i}: {layer_type.upper()}")
        print(f"    activation: '{activation}'")
        print(f"    output shape: {shape}")
        print(f"    num weight arrays: {len(weights)}")

        if layer_type.lower() == 'lstm':
            inspect_lstm_layer(weights)
        elif layer_type.lower() == 'gru':
            inspect_gru_layer(weights)
        elif layer_type.lower() == 'dense':
            inspect_dense_layer(weights)


def inspect_lstm_layer(weights: list) -> None:
    """Inspect LSTM layer weights in detail."""
    if len(weights) < 3:
        print(f"    ERROR: Expected 3 weight arrays, got {len(weights)}")
        return

    # Keras LSTM weights format:
    # weights[0]: kernel (input weights) - shape (input_size, 4*hidden_size)
    # weights[1]: recurrent_kernel - shape (hidden_size, 4*hidden_size)
    # weights[2]: bias - shape (4*hidden_size,) or (2, 4*hidden_size)

    kernel_shape = get_shape(weights[0])
    recurrent_shape = get_shape(weights[1])
    bias_shape = get_shape(weights[2])

    print(f"\n    [Kernel (input weights)]")
    print(f"      shape: {kernel_shape}")
    kernel = flatten_weights(weights[0])
    print(f"      flat size: {len(kernel)}")
    print(f"      first 8: {kernel[:8]}")
    print(f"      last 8: {kernel[-8:]}")

    # Infer hidden size and gate size
    if len(kernel_shape) >= 2:
        input_size = kernel_shape[0]
        gate_times_hidden = kernel_shape[-1]
        hidden_size = gate_times_hidden // 4  # LSTM has 4 gates
        print(f"      inferred: input_size={input_size}, hidden_size={hidden_size}")

    print(f"\n    [Recurrent kernel]")
    print(f"      shape: {recurrent_shape}")
    recurrent = flatten_weights(weights[1])
    print(f"      flat size: {len(recurrent)}")
    print(f"      first 8: {recurrent[:8]}")
    print(f"      last 8: {recurrent[-8:]}")

    print(f"\n    [Bias]")
    print(f"      shape: {bias_shape}")
    bias = flatten_weights(weights[2])
    print(f"      flat size: {len(bias)}")
    print(f"      first 8: {bias[:8]}")
    print(f"      last 8: {bias[-8:]}")

    # Check for dual bias (Keras sometimes stores [bias_input, bias_hidden])
    if len(bias_shape) == 2 and bias_shape[0] == 2:
        print(f"      NOTE: Dual bias format detected (Keras style)")
        print(f"            May need to sum bias_input + bias_hidden")

    # Gate order analysis
    print(f"\n    [Gate Order Analysis]")
    print(f"      Keras LSTM gate order: [i, f, c, o] (input, forget, cell, output)")
    print(f"      RTNeural expects:      [i, f, c, o] (same order)")

    if len(kernel) >= 48:  # LSTM-12 has 48 kernel weights
        hidden = 12
        print(f"\n      Assuming hidden_size={hidden}, gate slices of kernel:")
        for gate_idx, gate_name in enumerate(['i (input)', 'f (forget)', 'c (cell)', 'o (output)']):
            start = gate_idx * hidden
            end = start + hidden
            gate_slice = kernel[start:end]
            print(f"        gate {gate_idx} ({gate_name}): [{start}:{end}] = {gate_slice[:4]}...")


def inspect_gru_layer(weights: list) -> None:
    """Inspect GRU layer weights in detail."""
    if len(weights) < 3:
        print(f"    ERROR: Expected 3 weight arrays, got {len(weights)}")
        return

    for idx, name in enumerate(['kernel', 'recurrent_kernel', 'bias']):
        shape = get_shape(weights[idx])
        flat = flatten_weights(weights[idx])
        print(f"\n    [{name}]")
        print(f"      shape: {shape}")
        print(f"      flat size: {len(flat)}")
        print(f"      first 4: {flat[:4]}")
        print(f"      last 4: {flat[-4:]}")


def inspect_dense_layer(weights: list) -> None:
    """Inspect Dense layer weights in detail."""
    if len(weights) < 2:
        print(f"    ERROR: Expected 2 weight arrays, got {len(weights)}")
        return

    weight_shape = get_shape(weights[0])
    bias_shape = get_shape(weights[1])

    print(f"\n    [Dense weights]")
    print(f"      shape: {weight_shape}")
    w = flatten_weights(weights[0])
    print(f"      flat size: {len(w)}")
    print(f"      all values: {w}")

    print(f"\n    [Dense bias]")
    print(f"      shape: {bias_shape}")
    b = flatten_weights(weights[1])
    print(f"      flat size: {len(b)}")
    print(f"      all values: {b}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python inspect_model_json.py <model.json>")
        sys.exit(1)

    json_path = sys.argv[1]
    if not Path(json_path).exists():
        print(f"ERROR: File not found: {json_path}")
        sys.exit(1)

    inspect_model(json_path)


if __name__ == '__main__':
    main()
