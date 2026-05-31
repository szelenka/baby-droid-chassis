#!/bin/bash

# Usage: ./convert.sh <input_dir> <output_dir> [--flat] [--digits N]
# Options:
#   --flat        Disable recursive subfolder search
#   --digits N    Number of digits in output filename (default: 4)

set -e

usage() {
    echo "Usage: $0 <input_dir> <output_dir> [--flat] [--digits N]"
    exit 1
}

# --- Parse positional args first ---
if [ $# -lt 2 ]; then usage; fi

INPUT_DIR="$1"
OUTPUT_DIR="$2"
shift 2

# --- Defaults ---
RECURSIVE=true
DIGITS=4

# --- Parse optional flags ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --flat)      RECURSIVE=false; shift ;;
        --digits)    DIGITS="$2"; shift 2 ;;
        *)           echo "Unknown option: $1"; usage ;;
    esac
done

# --- Validate ---
if [ ! -d "$INPUT_DIR" ]; then
    echo "Error: Input directory not found: $INPUT_DIR"
    exit 1
fi

if ! command -v ffmpeg &>/dev/null; then
    echo "Error: ffmpeg not found. Install with: brew install ffmpeg"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

LOG_FILE="$OUTPUT_DIR/conversion_map.csv"
echo "destination,source_relative_path,source_filename" > "$LOG_FILE"

counter=1

convert_files() {
    local dir="$1"

    for file in "$dir"/*.wav "$dir"/*.WAV "$dir"/*.mp3 "$dir"/*.MP3; do
        [ -f "$file" ] || continue

        local rel_path="${file#$INPUT_DIR/}"
        local src_name="$(basename "$file")"
        local ext_lower
        ext_lower=$(echo "${file##*.}" | tr '[:upper:]' '[:lower:]')
        local output
        output=$(printf "%0${DIGITS}d.mp3" "$counter")

        if [[ "$ext_lower" == "mp3" ]]; then
            # Already MP3 — copy and rename, no re-encoding
            if cp "$file" "$OUTPUT_DIR/$output"; then
                echo "$output,\"$rel_path\",\"$src_name\",copied" >> "$LOG_FILE"
                echo "⊕ [$output] $rel_path (copied)"
            else
                echo "✗ FAILED (copy): $rel_path"
            fi
        else
            # WAV — convert via ffmpeg
            if ffmpeg -i "$file" -codec:a libmp3lame -qscale:a 2 "$OUTPUT_DIR/$output" -y 2>/dev/null; then
                echo "$output,\"$rel_path\",\"$src_name\",converted" >> "$LOG_FILE"
                echo "✓ [$output] $rel_path (converted)"
            else
                echo "✗ FAILED (convert): $rel_path"
            fi
        fi

        ((counter++))
    done

    if [ "$RECURSIVE" = true ]; then
        for subdir in "$dir"/*/; do
            [ -d "$subdir" ] || continue
            convert_files "$subdir"
        done
    fi
}

convert_files "$INPUT_DIR"

echo ""
echo "Done! Converted $((counter-1)) files."
echo "Translation map: $LOG_FILE"
