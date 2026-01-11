#!/usr/bin/env python3
"""
Convert WAV impulse response files to C++ constexpr headers.

Features:
- Resamples to 48kHz if needed
- Converts stereo to mono (sum and average)
- Normalizes peak amplitude to 1.0
- Truncates to max 2048 samples

Usage:
    python convert_ir_to_cpp.py <input.wav> [output_dir]
    python convert_ir_to_cpp.py --batch <input_dir> [output_dir]

Output: C++ header file with constexpr float array for IR convolution.
"""

import sys
import os
import re
import argparse
import struct
from pathlib import Path

# Try to import scipy for resampling, fall back to simple interpolation
try:
    from scipy.io import wavfile
    from scipy import signal
    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False

# Try numpy for array operations
try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False


MAX_IR_LENGTH = 2048
TARGET_SAMPLE_RATE = 48000


def sanitize_name(name: str) -> str:
    """Convert filename to valid C++ identifier."""
    name = Path(name).stem
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    if name[0].isdigit():
        name = '_' + name
    return name


def read_wav_simple(path: str) -> tuple:
    """Read WAV file without scipy (basic implementation)."""
    with open(path, 'rb') as f:
        # RIFF header
        riff = f.read(4)
        if riff != b'RIFF':
            raise ValueError("Not a WAV file")

        f.read(4)  # file size
        wave = f.read(4)
        if wave != b'WAVE':
            raise ValueError("Not a WAV file")

        # Find fmt chunk
        fmt_found = False
        data_found = False
        sample_rate = 0
        num_channels = 0
        bits_per_sample = 0
        audio_data = []

        while True:
            chunk_id = f.read(4)
            if len(chunk_id) < 4:
                break

            chunk_size = struct.unpack('<I', f.read(4))[0]

            if chunk_id == b'fmt ':
                fmt_data = f.read(chunk_size)
                audio_format = struct.unpack('<H', fmt_data[0:2])[0]
                num_channels = struct.unpack('<H', fmt_data[2:4])[0]
                sample_rate = struct.unpack('<I', fmt_data[4:8])[0]
                bits_per_sample = struct.unpack('<H', fmt_data[14:16])[0]
                fmt_found = True

            elif chunk_id == b'data':
                raw_data = f.read(chunk_size)

                # Convert to float samples
                if bits_per_sample == 16:
                    samples = struct.unpack(f'<{len(raw_data)//2}h', raw_data)
                    audio_data = [s / 32768.0 for s in samples]
                elif bits_per_sample == 24:
                    # 24-bit is trickier
                    samples = []
                    for i in range(0, len(raw_data), 3):
                        b = raw_data[i:i+3]
                        val = struct.unpack('<i', b + (b'\xff' if b[2] & 0x80 else b'\x00'))[0]
                        samples.append(val / 8388608.0)
                    audio_data = samples
                elif bits_per_sample == 32:
                    # Assume float
                    samples = struct.unpack(f'<{len(raw_data)//4}f', raw_data)
                    audio_data = list(samples)
                else:
                    raise ValueError(f"Unsupported bit depth: {bits_per_sample}")

                data_found = True
            else:
                f.read(chunk_size)  # Skip unknown chunk

        if not fmt_found or not data_found:
            raise ValueError("Invalid WAV file structure")

        return sample_rate, num_channels, audio_data


def simple_resample(data: list, src_rate: int, dst_rate: int) -> list:
    """Simple linear interpolation resampling."""
    if src_rate == dst_rate:
        return data

    ratio = dst_rate / src_rate
    new_length = int(len(data) * ratio)
    result = []

    for i in range(new_length):
        src_pos = i / ratio
        idx = int(src_pos)
        frac = src_pos - idx

        if idx + 1 < len(data):
            val = data[idx] * (1 - frac) + data[idx + 1] * frac
        else:
            val = data[idx] if idx < len(data) else 0
        result.append(val)

    return result


def load_ir(path: str) -> tuple:
    """
    Load and process IR file.

    Returns (samples, original_sample_rate, original_channels, original_length)
    """
    if HAS_SCIPY:
        sample_rate, data = wavfile.read(path)

        # Convert to float
        if data.dtype == np.int16:
            data = data.astype(np.float32) / 32768.0
        elif data.dtype == np.int32:
            data = data.astype(np.float32) / 2147483648.0
        elif data.dtype != np.float32 and data.dtype != np.float64:
            data = data.astype(np.float32)

        num_channels = 1 if len(data.shape) == 1 else data.shape[1]
        original_length = len(data)

        # Convert stereo to mono
        if num_channels > 1:
            data = np.mean(data, axis=1)

        # Resample if needed
        if sample_rate != TARGET_SAMPLE_RATE:
            num_samples = int(len(data) * TARGET_SAMPLE_RATE / sample_rate)
            data = signal.resample(data, num_samples)

        samples = data.tolist()
    else:
        # Fallback without scipy
        sample_rate, num_channels, raw_samples = read_wav_simple(path)
        original_length = len(raw_samples) // num_channels

        # Convert stereo to mono
        if num_channels > 1:
            samples = []
            for i in range(0, len(raw_samples), num_channels):
                mono = sum(raw_samples[i:i+num_channels]) / num_channels
                samples.append(mono)
        else:
            samples = raw_samples

        # Resample if needed
        if sample_rate != TARGET_SAMPLE_RATE:
            samples = simple_resample(samples, sample_rate, TARGET_SAMPLE_RATE)

    original_sample_rate = sample_rate
    original_channels = num_channels if 'num_channels' in dir() else (1 if len(data.shape) == 1 else data.shape[1])

    return samples, original_sample_rate, original_channels, original_length


def normalize_ir(samples: list) -> list:
    """Normalize IR to peak amplitude of 1.0."""
    peak = max(abs(s) for s in samples)
    if peak > 0:
        return [s / peak for s in samples]
    return samples


def truncate_ir(samples: list, max_length: int = MAX_IR_LENGTH) -> list:
    """Truncate IR to maximum length with fade-out."""
    if len(samples) <= max_length:
        return samples

    # Apply short fade-out at end to avoid clicks
    result = samples[:max_length]
    fade_len = min(64, max_length // 8)
    for i in range(fade_len):
        fade = 1.0 - (i / fade_len)
        result[max_length - fade_len + i] *= fade

    return result


def format_array(values: list, name: str, indent: str = "    ") -> str:
    """Format a flat list of floats as a C++ constexpr array."""
    lines = [f"constexpr float {name}[] = {{"]

    # Format 6 values per line for readability
    for i in range(0, len(values), 6):
        chunk = values[i:i+6]
        formatted = ", ".join(f"{v:+.8f}f" for v in chunk)
        if i + 6 < len(values):
            formatted += ","
        lines.append(f"{indent}{formatted}")

    lines.append("};")
    return "\n".join(lines)


def generate_header(samples: list, identifier: str, display_name: str,
                   original_rate: int, original_channels: int,
                   original_length: int, source_file: str) -> str:
    """Generate C++ header content for an IR."""

    header = f'''#pragma once
// Auto-generated from IR file: {source_file}
// Original: {original_rate}Hz, {original_channels}ch, {original_length} samples
// Processed: {TARGET_SAMPLE_RATE}Hz, mono, {len(samples)} samples, normalized
// DO NOT EDIT - regenerate using convert_ir_to_cpp.py

#include <cstddef>

namespace EmbeddedIRs {{
namespace {identifier} {{

constexpr const char* kName = "{display_name}";
constexpr int kSampleRate = {TARGET_SAMPLE_RATE};
constexpr int kLength = {len(samples)};

// Impulse response samples (mono, normalized)
{format_array(samples, 'kSamples')}

}} // namespace {identifier}
}} // namespace EmbeddedIRs
'''
    return header


def convert_file(input_path: str, output_dir: str, force: bool = False) -> bool:
    """Convert a single IR file. Returns True on success."""
    try:
        # Load and process
        samples, orig_rate, orig_channels, orig_length = load_ir(input_path)

        # Normalize
        samples = normalize_ir(samples)

        # Truncate
        if len(samples) > MAX_IR_LENGTH:
            print(f"  Truncating from {len(samples)} to {MAX_IR_LENGTH} samples")
        samples = truncate_ir(samples, MAX_IR_LENGTH)

        # Generate output
        identifier = sanitize_name(input_path)
        display_name = Path(input_path).stem.replace('_', ' ').replace('-', ' ')
        header = generate_header(
            samples, identifier, display_name,
            orig_rate, orig_channels, orig_length,
            Path(input_path).name
        )

        # Write output
        output_path = Path(output_dir) / f"{identifier}.h"
        output_path.parent.mkdir(parents=True, exist_ok=True)

        with open(output_path, 'w') as f:
            f.write(header)

        info = f"{orig_rate}Hz/{orig_channels}ch -> {len(samples)} samples"
        print(f"OK {input_path} ({info}) -> {output_path}")
        return True

    except Exception as e:
        print(f"ERROR {input_path}: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    parser = argparse.ArgumentParser(description='Convert WAV IRs to C++ headers')
    parser.add_argument('input', help='Input WAV file or directory (with --batch)')
    parser.add_argument('output_dir', nargs='?', default='./embedded/irs',
                       help='Output directory (default: ./embedded/irs)')
    parser.add_argument('--batch', action='store_true',
                       help='Process all .wav files in input directory')
    parser.add_argument('--force', '-f', action='store_true',
                       help='Overwrite existing files')

    args = parser.parse_args()

    if not HAS_SCIPY:
        print("Warning: scipy not available, using basic WAV reader and resampler")

    if args.batch:
        input_dir = Path(args.input)
        if not input_dir.is_dir():
            print(f"Error: {args.input} is not a directory")
            sys.exit(1)

        wav_files = list(input_dir.glob('*.wav')) + list(input_dir.glob('*.WAV'))
        if not wav_files:
            print(f"No .wav files found in {args.input}")
            sys.exit(1)

        success = 0
        for wav_file in sorted(wav_files):
            if convert_file(str(wav_file), args.output_dir, args.force):
                success += 1

        print(f"\nConverted {success}/{len(wav_files)} IRs")
    else:
        if not Path(args.input).is_file():
            print(f"Error: {args.input} is not a file")
            sys.exit(1)

        if not convert_file(args.input, args.output_dir, args.force):
            sys.exit(1)


if __name__ == '__main__':
    main()
