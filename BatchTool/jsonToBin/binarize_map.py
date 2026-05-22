"""
binarize_map.py — Convert map JSON files to high-performance MessagePack binary (.bin) files.

Usage:
    python binarize_map.py [--input PATH_OR_DIR]

It will automatically discover all .json map files (or split chunks) and convert them to .bin.
The game engine MapLoader will automatically detect and load these .bin files for a 10x speedup!
"""

import sys
import os
import json
import argparse
from pathlib import Path

def check_and_install_dependencies():
    try:
        import msgpack
    except ImportError:
        print("[binarize_map] 'msgpack' library is not installed.")
        print("[binarize_map] Attempting to install 'msgpack' automatically via pip...")
        import subprocess
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", "msgpack"])
            import msgpack
            print("[binarize_map] 'msgpack' installed successfully!")
        except Exception as e:
            print(f"[binarize_map] Failed to install 'msgpack' automatically: {e}")
            print("[binarize_map] Please run: pip install msgpack")
            sys.exit(1)
    return msgpack

def binarize_file(msgpack, json_path: Path):
    bin_path = json_path.with_suffix(".bin")
    print(f"[binarize_map] Converting: {json_path.name} -> {bin_path.name}")
    
    try:
        with open(json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            
        # Preserve Base64 string format inside MsgPack to avoid C++ from_msgpack deserialization hangs.
        # The C++ MapLoader already contains a fallback to decode Base64 strings using Base64_Decode.
        pass
            
        packed = msgpack.packb(data, use_bin_type=True)
        
        with open(bin_path, "wb") as f:
            f.write(packed)
            
        orig_size = json_path.stat().st_size / (1024 * 1024)
        bin_size = bin_path.stat().st_size / (1024 * 1024)
        ratio = (bin_size / orig_size) * 100 if orig_size > 0 else 0
        print(f"               Done! Size: {orig_size:.2f}MB -> {bin_size:.2f}MB ({ratio:.1f}%)")
    except Exception as e:
        print(f"               [Error] Failed to convert {json_path.name}: {e}")

def main():
    msgpack = check_and_install_dependencies()
    
    parser = argparse.ArgumentParser(description="Map JSON to MsgPack Binary Converter")
    parser.add_argument("--input", default="../../Client/Bin/Resources/Map/SaveJson", 
                        help="Path to a single JSON file or directory containing JSON files")
    args = parser.parse_args()
    
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"[binarize_map] Error: Path not found: {input_path}")
        sys.exit(1)
        
    if input_path.is_file():
        if input_path.suffix.lower() == ".json":
            binarize_file(msgpack, input_path)
        else:
            print(f"[binarize_map] Error: File is not a JSON file: {input_path}")
    else:
        # Search all json files recursively in chunks and main folder
        json_files = list(input_path.glob("**/*.json"))
        # Exclude manifest.json as it's small metadata
        json_files = [f for f in json_files if f.name != "manifest.json"]
        
        if not json_files:
            print(f"[binarize_map] No JSON files found in {input_path}")
            return
            
        print(f"[binarize_map] Found {len(json_files)} JSON files to binarize...")
        for jf in json_files:
            binarize_file(msgpack, jf)
            
    print("[binarize_map] All conversions completed!")

if __name__ == "__main__":
    main()
