#!/usr/bin/env python3
"""
quantize_models.py
Datalake 3.0 — Model Quantization Pipeline
Hackathon 7.0 | NHAI

Pipeline to quantize floating-point ONNX models to INT8 to meet the 
strict <20MB total footprint requirement. Uses ONNX Runtime quantization.
"""

import os
import sys

try:
    import onnx
    from onnxruntime.quantization import quantize_dynamic, QuantType
except ImportError:
    print("Error: Required packages not found.")
    print("Please run: pip install onnx onnxruntime")
    sys.exit(1)

def quantize_model(input_model_path, output_model_path):
    print(f"Quantizing {input_model_path} to {output_model_path}...")
    try:
        quantize_dynamic(
            model_input=input_model_path,
            model_output=output_model_path,
            weight_type=QuantType.QInt8
        )

        # Check size reduction
        orig_size = os.path.getsize(input_model_path) / (1024 * 1024)
        new_size = os.path.getsize(output_model_path) / (1024 * 1024)
        print(f"Success! Size reduced from {orig_size:.2f}MB to {new_size:.2f}MB")
    except Exception as e:
        print(f"Error quantizing {input_model_path}: {e}")
        sys.exit(1)

def main():
    models_dir = os.path.join(os.path.dirname(__file__), "../models")
    os.makedirs(models_dir, exist_ok=True)

    # Expected models to be downloaded and placed in the models directory
    models = [
        ("blazeface.onnx", "blazeface_int8.onnx"),
        ("landmark68.onnx", "landmark68_int8.onnx"),
        ("mobilefacenet.onnx", "mobilefacenet_int8.onnx")
    ]

    for in_name, out_name in models:
        in_path = os.path.join(models_dir, in_name)
        out_path = os.path.join(models_dir, out_name)

        if not os.path.exists(in_path):
            print(f"Warning: Input model {in_path} not found. Skipping.")
            continue

        quantize_model(in_path, out_path)

if __name__ == "__main__":
    main()
