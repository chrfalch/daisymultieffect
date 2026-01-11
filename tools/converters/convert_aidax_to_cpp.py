#!/usr/bin/env python3
"""
Convert AIDA-X neural amp model JSON files to C++ constexpr headers.

Supports LSTM-12 models only (for both VST and firmware).

Usage:
    python convert_aidax_to_cpp.py <input.json> [output_dir]
    python convert_aidax_to_cpp.py --batch <input_dir> [output_dir]

Output: C++ header file with constexpr float arrays for RTNeural weight loading.
"""

import json
import sys
import os
import re
import argparse
from pathlib import Path


def sanitize_name(name: str) -> str:
    """Convert filename to valid C++ identifier."""
    # Remove extension and path
    name = Path(name).stem
    # Replace invalid chars with underscore
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    # Ensure doesn't start with digit
    if name[0].isdigit():
        name = '_' + name
    return name


def format_array(values: list, name: str, indent: str = "    ") -> str:
    """Format a flat list of floats as a C++ constexpr array."""
    lines = [f"constexpr float {name}[] = {{"]

    # Format 8 values per line
    for i in range(0, len(values), 8):
        chunk = values[i:i+8]
        formatted = ", ".join(f"{v:.8f}f" for v in chunk)
        if i + 8 < len(values):
            formatted += ","
        lines.append(f"{indent}{formatted}")

    lines.append("};")
    return "\n".join(lines)


def flatten_weights(weights: list) -> list:
    """Flatten nested weight arrays to 1D."""
    result = []
    for item in weights:
        if isinstance(item, list):
            result.extend(flatten_weights(item))
        else:
            result.append(item)
    return result


def parse_model(json_path: str) -> dict:
    """
    Parse AIDA-X model JSON and extract weights.

    Returns dict with:
        - name: model name
        - architecture: 'lstm' or 'gru'
        - hidden_size: int
        - weights: dict of weight arrays
    """
    with open(json_path, 'r') as f:
        data = json.load(f)

    # Extract metadata
    metadata = data.get('metadata', {})
    name = metadata.get('name', Path(json_path).stem)

    # Parse layers
    layers = data.get('layers', [])
    if len(layers) < 2:
        raise ValueError(f"Expected at least 2 layers, got {len(layers)}")

    # First layer: LSTM or GRU
    rnn_layer = layers[0]
    rnn_type = rnn_layer.get('type', '').lower()
    if rnn_type not in ('lstm', 'gru'):
        raise ValueError(f"First layer must be LSTM or GRU, got: {rnn_type}")

    rnn_shape = rnn_layer.get('shape', [])
    hidden_size = rnn_shape[-1] if rnn_shape else 0

    # Second layer: Dense output
    dense_layer = layers[1]
    if dense_layer.get('type', '').lower() != 'dense':
        raise ValueError("Second layer must be Dense")

    # Extract weights
    rnn_weights = rnn_layer.get('weights', [])
    dense_weights = dense_layer.get('weights', [])

    # LSTM/GRU weights format (Keras/TensorFlow style):
    # weights[0]: kernel (input weights) - shape (input_size, 4*hidden_size) for LSTM
    # weights[1]: recurrent_kernel - shape (hidden_size, 4*hidden_size) for LSTM
    # weights[2]: bias - shape (4*hidden_size,) for LSTM (or 2 biases concatenated)

    if len(rnn_weights) < 3:
        raise ValueError(f"Expected 3 RNN weight arrays, got {len(rnn_weights)}")

    # Flatten all weights
    kernel = flatten_weights(rnn_weights[0])  # input weights
    recurrent = flatten_weights(rnn_weights[1])  # recurrent weights
    bias = flatten_weights(rnn_weights[2])  # biases

    dense_w = flatten_weights(dense_weights[0])
    dense_b = flatten_weights(dense_weights[1])

    # Validate sizes
    gate_size = 4 if rnn_type == 'lstm' else 3  # LSTM has 4 gates, GRU has 3
    expected_kernel_size = 1 * gate_size * hidden_size  # input_size=1
    expected_recurrent_size = hidden_size * gate_size * hidden_size
    expected_bias_size = gate_size * hidden_size

    if len(kernel) != expected_kernel_size:
        raise ValueError(f"Kernel size mismatch: {len(kernel)} vs {expected_kernel_size}")
    if len(recurrent) != expected_recurrent_size:
        raise ValueError(f"Recurrent size mismatch: {len(recurrent)} vs {expected_recurrent_size}")

    return {
        'name': name,
        'architecture': rnn_type,
        'hidden_size': hidden_size,
        'weights': {
            'kernel': kernel,
            'recurrent': recurrent,
            'bias': bias,
            'dense_w': dense_w,
            'dense_b': dense_b,
        }
    }


def generate_header(model: dict, identifier: str) -> str:
    """Generate C++ header content for a model."""
    arch = model['architecture'].upper()
    hidden = model['hidden_size']
    name = model['name']
    weights = model['weights']

    header = f'''#pragma once
// Auto-generated from AIDA-X model: {name}
// Architecture: {arch}-{hidden}
// DO NOT EDIT - regenerate using convert_aidax_to_cpp.py

#include <cstddef>

namespace EmbeddedModels {{
namespace {identifier} {{

constexpr const char* kName = "{name}";
constexpr const char* kArchitecture = "{arch}";
constexpr int kHiddenSize = {hidden};
constexpr int kInputSize = 1;
constexpr int kOutputSize = 1;

// Input kernel weights: shape (input_size, {4 if arch == 'LSTM' else 3} * hidden_size)
{format_array(weights['kernel'], 'kKernel')}
constexpr size_t kKernelSize = sizeof(kKernel) / sizeof(kKernel[0]);

// Recurrent kernel weights: shape (hidden_size, {4 if arch == 'LSTM' else 3} * hidden_size)
{format_array(weights['recurrent'], 'kRecurrent')}
constexpr size_t kRecurrentSize = sizeof(kRecurrent) / sizeof(kRecurrent[0]);

// Bias: shape ({4 if arch == 'LSTM' else 3} * hidden_size,)
{format_array(weights['bias'], 'kBias')}
constexpr size_t kBiasSize = sizeof(kBias) / sizeof(kBias[0]);

// Dense output weights: shape (hidden_size, output_size)
{format_array(weights['dense_w'], 'kDenseW')}
constexpr size_t kDenseWSize = sizeof(kDenseW) / sizeof(kDenseW[0]);

// Dense output bias: shape (output_size,)
{format_array(weights['dense_b'], 'kDenseB')}
constexpr size_t kDenseBSize = sizeof(kDenseB) / sizeof(kDenseB[0]);

}} // namespace {identifier}
}} // namespace EmbeddedModels
'''
    return header


def validate_model(model: dict) -> tuple[bool, str]:
    """
    Validate model is compatible with firmware requirements.

    Returns (is_valid, error_message)
    """
    arch = model['architecture']
    hidden = model['hidden_size']

    # Only allow LSTM-12 for consistency
    if arch != 'lstm':
        return False, f"Only LSTM architecture supported, got: {arch}"

    if hidden != 12:
        return False, f"Only hidden_size=12 supported, got: {hidden}"

    return True, ""


def convert_file(input_path: str, output_dir: str, force: bool = False) -> bool:
    """Convert a single model file. Returns True on success."""
    try:
        model = parse_model(input_path)

        # Validate
        valid, error = validate_model(model)
        if not valid:
            print(f"SKIP {input_path}: {error}")
            return False

        # Generate output
        identifier = sanitize_name(input_path)
        header = generate_header(model, identifier)

        # Write output
        output_path = Path(output_dir) / f"{identifier}.h"
        output_path.parent.mkdir(parents=True, exist_ok=True)

        with open(output_path, 'w') as f:
            f.write(header)

        print(f"OK {input_path} -> {output_path}")
        return True

    except Exception as e:
        print(f"ERROR {input_path}: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description='Convert AIDA-X models to C++ headers')
    parser.add_argument('input', help='Input JSON file or directory (with --batch)')
    parser.add_argument('output_dir', nargs='?', default='./embedded/models',
                       help='Output directory (default: ./embedded/models)')
    parser.add_argument('--batch', action='store_true',
                       help='Process all .json files in input directory')
    parser.add_argument('--force', '-f', action='store_true',
                       help='Overwrite existing files')

    args = parser.parse_args()

    if args.batch:
        input_dir = Path(args.input)
        if not input_dir.is_dir():
            print(f"Error: {args.input} is not a directory")
            sys.exit(1)

        json_files = list(input_dir.glob('*.json'))
        if not json_files:
            print(f"No .json files found in {args.input}")
            sys.exit(1)

        success = 0
        for json_file in sorted(json_files):
            if convert_file(str(json_file), args.output_dir, args.force):
                success += 1

        print(f"\nConverted {success}/{len(json_files)} models")
    else:
        if not Path(args.input).is_file():
            print(f"Error: {args.input} is not a file")
            sys.exit(1)

        if not convert_file(args.input, args.output_dir, args.force):
            sys.exit(1)


if __name__ == '__main__':
    main()
