#!/usr/bin/env python3
"""
Convert Mercury NAM model weights from std::vector<float> to constexpr float arrays.

This script reads the Mercury nam_model_data.h file and converts the runtime-initialized
weights to compile-time constexpr arrays suitable for embedded use.

Usage:
    python3 convert_nam_weights.py
"""

import re
import os

INPUT_FILE = "../firmware/src/effects/nam_model_data.h"
OUTPUT_FILE = "../firmware/src/effects/nam_pico_weights.h"

MODEL_NAMES = [
    ("NamModel1", "kMatchlessWeights", "Matchless SC30"),
    ("NamModel2", "kTwinReverbWeights", "Twin Reverb"),
    ("NamModel3", "kDumbleLowWeights", "Dumble Low Gain"),
    ("NamModel4", "kDumbleHighWeights", "Dumble High Gain"),
    ("NamModel5", "kEthosLeadWeights", "Ethos Lead"),
    ("NamModel6", "kJcm800Weights", "JCM800"),
    ("NamModel7", "kMesaWeights", "Mesa Mark III"),
    ("NamModel8", "kSplawnWeights", "Splawn Lead"),
    ("NamModel9", "kPrsArchonWeights", "PRS Archon"),
]

def extract_weights(content, model_var):
    """Extract weights array from Mercury format."""
    # Pattern: NamModel1.weights =  {-0.816..., 0.640..., ...};
    pattern = rf'{model_var}\.weights\s*=\s*\{{([^}}]+)\}}'
    match = re.search(pattern, content)
    if not match:
        print(f"Warning: Could not find weights for {model_var}")
        return None

    weights_str = match.group(1)
    # Parse float values
    weights = []
    for val in weights_str.split(','):
        val = val.strip()
        if val:
            try:
                weights.append(float(val))
            except ValueError:
                continue
    return weights

def format_weights_array(weights, name):
    """Format weights as constexpr float array."""
    lines = []
    lines.append(f"constexpr float {name}[] = {{")

    # Format in rows of 8 values
    for i in range(0, len(weights), 8):
        chunk = weights[i:i+8]
        formatted = ", ".join(f"{w:.8f}f" for w in chunk)
        if i + 8 < len(weights):
            formatted += ","
        lines.append(f"    {formatted}")

    lines.append("};")
    lines.append(f"constexpr size_t {name}Count = sizeof({name}) / sizeof(float);")
    return "\n".join(lines)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    input_path = os.path.join(script_dir, INPUT_FILE)
    output_path = os.path.join(script_dir, OUTPUT_FILE)

    print(f"Reading {input_path}...")
    with open(input_path, 'r') as f:
        content = f.read()

    output_lines = [
        "/**",
        " * @file nam_pico_weights.h",
        " * @brief NAM Pico model weights converted to constexpr arrays",
        " *",
        " * Auto-generated from Mercury's nam_model_data.h",
        " * Source: https://github.com/GuitarML/Mercury",
        " * License: MIT",
        " */",
        "#pragma once",
        "",
        "#include <cstddef>",
        "",
        "#if defined(DAISY_SEED_BUILD) && defined(HAS_RTNEURAL) && HAS_RTNEURAL",
        "",
        "namespace nam_wavenet {",
        "",
    ]

    registry_entries = []

    for model_var, weight_name, display_name in MODEL_NAMES:
        weights = extract_weights(content, model_var)
        if weights:
            print(f"  {model_var}: {len(weights)} weights -> {weight_name}")
            output_lines.append(f"// {display_name}")
            output_lines.append(format_weights_array(weights, weight_name))
            output_lines.append("")
            registry_entries.append((weight_name, display_name))
        else:
            print(f"  {model_var}: SKIPPED (not found)")

    # Generate registry
    output_lines.append("// Model registry")
    output_lines.append("struct NamPicoModelEntry {")
    output_lines.append("    const char* name;")
    output_lines.append("    const float* weights;")
    output_lines.append("    size_t weightCount;")
    output_lines.append("};")
    output_lines.append("")
    output_lines.append("constexpr NamPicoModelEntry kNamPicoModels[] = {")
    for weight_name, display_name in registry_entries:
        output_lines.append(f'    {{"{display_name}", {weight_name}, {weight_name}Count}},')
    output_lines.append("};")
    output_lines.append("")
    output_lines.append(f"constexpr size_t kNamPicoModelCount = {len(registry_entries)};")
    output_lines.append("")
    output_lines.append("inline const NamPicoModelEntry* GetNamPicoModel(size_t index) {")
    output_lines.append("    return (index < kNamPicoModelCount) ? &kNamPicoModels[index] : nullptr;")
    output_lines.append("}")
    output_lines.append("")
    output_lines.append("} // namespace nam_wavenet")
    output_lines.append("")
    output_lines.append("#endif // DAISY_SEED_BUILD && HAS_RTNEURAL")
    output_lines.append("")

    output_content = "\n".join(output_lines)

    print(f"\nWriting {output_path}...")
    with open(output_path, 'w') as f:
        f.write(output_content)

    print(f"Done! Generated {len(registry_entries)} model entries.")

if __name__ == "__main__":
    main()
