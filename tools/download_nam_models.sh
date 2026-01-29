#!/bin/bash
# Download NAM Pico model weights from GuitarML/Mercury
# These are WaveNet "Pico" models (channels=2) optimized for embedded

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/../firmware/src/effects"
OUTPUT_FILE="${OUTPUT_DIR}/nam_model_data.h"

echo "Downloading Mercury NAM model weights..."

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Download the model_data_nam.h file from Mercury
curl -sL "https://raw.githubusercontent.com/GuitarML/Mercury/main/model_data_nam.h" -o "$OUTPUT_FILE"

# Check if download succeeded
if [ ! -s "$OUTPUT_FILE" ]; then
    echo "Error: Failed to download model weights"
    exit 1
fi

echo "Downloaded to: $OUTPUT_FILE"

# Show model count
MODEL_COUNT=$(grep -c "NamModel" "$OUTPUT_FILE" | head -1 || echo "0")
echo "Found approximately $MODEL_COUNT models"

# Show file size
FILE_SIZE=$(wc -c < "$OUTPUT_FILE" | tr -d ' ')
echo "File size: $FILE_SIZE bytes"

echo ""
echo "Models included (from Mercury project):"
echo "  - Matchless (clean)"
echo "  - Twin Reverb (clean)"
echo "  - Dumble Low Gain"
echo "  - Dumble High Gain"
echo "  - Splawn (high gain)"
echo "  - PRS Archon (high gain)"
echo "  - Bassman"
echo "  - 5150 (high gain)"
echo ""
echo "Done! Model weights ready for firmware build."
