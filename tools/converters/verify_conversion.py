#!/usr/bin/env python3
"""
Verify that a converted C++ header matches the original AIDA-X JSON model.

Compares all weight values element-by-element and reports any mismatches.

Usage:
    python verify_conversion.py <model.json> <generated_header.h>

Example:
    python verify_conversion.py \
        "../../tmp/Deer Ink Studios/tw40_california_clean_deerinkstudios.json" \
        "../../core/effects/embedded/models/tw40_california_clean_deerinkstudios.h"
"""

import json
import re
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


def parse_json_model(json_path: str) -> dict:
    """Parse AIDA-X JSON and extract weight arrays."""
    with open(json_path, 'r') as f:
        data = json.load(f)

    layers = data.get('layers', [])
    if len(layers) < 2:
        raise ValueError(f"Expected at least 2 layers, got {len(layers)}")

    lstm_layer = layers[0]
    dense_layer = layers[1]

    if lstm_layer.get('type', '').lower() not in ('lstm', 'gru'):
        raise ValueError(f"First layer must be LSTM or GRU")

    lstm_weights = lstm_layer.get('weights', [])
    dense_weights = dense_layer.get('weights', [])

    return {
        'kernel': flatten_weights(lstm_weights[0]),
        'recurrent': flatten_weights(lstm_weights[1]),
        'bias': flatten_weights(lstm_weights[2]),
        'dense_w': flatten_weights(dense_weights[0]),
        'dense_b': flatten_weights(dense_weights[1]),
    }


def parse_cpp_header(header_path: str) -> dict:
    """Parse generated C++ header and extract weight arrays."""
    with open(header_path, 'r') as f:
        content = f.read()

    def extract_array(name: str) -> np.ndarray:
        """Extract a constexpr float array from C++ code."""
        # Match: constexpr float kName[] = { ... };
        pattern = rf'constexpr\s+float\s+{name}\[\]\s*=\s*\{{\s*([^}}]+)\s*\}};'
        match = re.search(pattern, content, re.DOTALL)
        if not match:
            raise ValueError(f"Could not find array '{name}' in header")

        values_str = match.group(1)
        # Parse float values (with optional 'f' suffix)
        values = []
        for v in re.findall(r'[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?f?', values_str):
            # Remove 'f' suffix if present
            v = v.rstrip('f')
            values.append(float(v))

        return np.array(values, dtype=np.float32)

    return {
        'kernel': extract_array('kKernel'),
        'recurrent': extract_array('kRecurrent'),
        'bias': extract_array('kBias'),
        'dense_w': extract_array('kDenseW'),
        'dense_b': extract_array('kDenseB'),
    }


def compare_arrays(name: str, json_arr: np.ndarray, cpp_arr: np.ndarray,
                   tolerance: float = 1e-6) -> list:
    """Compare two arrays and return list of mismatches."""
    mismatches = []

    if len(json_arr) != len(cpp_arr):
        mismatches.append({
            'type': 'size_mismatch',
            'name': name,
            'json_size': len(json_arr),
            'cpp_size': len(cpp_arr),
        })
        return mismatches

    diff = np.abs(json_arr - cpp_arr)
    bad_indices = np.where(diff > tolerance)[0]

    for idx in bad_indices:
        mismatches.append({
            'type': 'value_mismatch',
            'name': name,
            'index': int(idx),
            'json_value': float(json_arr[idx]),
            'cpp_value': float(cpp_arr[idx]),
            'diff': float(diff[idx]),
        })

    return mismatches


def verify_conversion(json_path: str, header_path: str) -> bool:
    """
    Verify conversion from JSON to C++ header.

    Returns True if all values match, False otherwise.
    """
    print("=" * 70)
    print("Neural Amp Model Conversion Verification")
    print("=" * 70)
    print(f"\n  JSON:   {json_path}")
    print(f"  Header: {header_path}\n")

    try:
        json_weights = parse_json_model(json_path)
        cpp_weights = parse_cpp_header(header_path)
    except Exception as e:
        print(f"ERROR: Failed to parse files: {e}")
        return False

    all_pass = True
    total_values = 0
    total_mismatches = 0

    for name in ['kernel', 'recurrent', 'bias', 'dense_w', 'dense_b']:
        json_arr = json_weights[name]
        cpp_arr = cpp_weights[name]
        total_values += len(json_arr)

        mismatches = compare_arrays(name, json_arr, cpp_arr)

        if not mismatches:
            print(f"  ✓ {name}: {len(json_arr)} values match")
        else:
            all_pass = False
            total_mismatches += len(mismatches)

            # Check for size mismatch first
            size_mismatch = [m for m in mismatches if m['type'] == 'size_mismatch']
            if size_mismatch:
                m = size_mismatch[0]
                print(f"  ✗ {name}: SIZE MISMATCH - JSON has {m['json_size']}, C++ has {m['cpp_size']}")
            else:
                print(f"  ✗ {name}: {len(mismatches)} value mismatches out of {len(json_arr)}")

                # Show first few mismatches
                for m in mismatches[:5]:
                    print(f"      [{m['index']}]: JSON={m['json_value']:.8f}, "
                          f"C++={m['cpp_value']:.8f}, diff={m['diff']:.2e}")
                if len(mismatches) > 5:
                    print(f"      ... and {len(mismatches) - 5} more mismatches")

    print("\n" + "=" * 70)
    if all_pass:
        print(f"PASS: All {total_values} values match!")
    else:
        print(f"FAIL: {total_mismatches} mismatches found out of {total_values} values")
    print("=" * 70)

    return all_pass


def main():
    if len(sys.argv) < 3:
        print("Usage: python verify_conversion.py <model.json> <generated_header.h>")
        print("\nExample:")
        print('  python verify_conversion.py \\')
        print('      "../../tmp/Deer Ink Studios/tw40_california_clean_deerinkstudios.json" \\')
        print('      "../../core/effects/embedded/models/tw40_california_clean_deerinkstudios.h"')
        sys.exit(1)

    json_path = sys.argv[1]
    header_path = sys.argv[2]

    if not Path(json_path).exists():
        print(f"ERROR: JSON file not found: {json_path}")
        sys.exit(1)

    if not Path(header_path).exists():
        print(f"ERROR: Header file not found: {header_path}")
        sys.exit(1)

    success = verify_conversion(json_path, header_path)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
