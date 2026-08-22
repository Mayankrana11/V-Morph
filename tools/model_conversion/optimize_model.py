#!/usr/bin/env python3
"""
Model optimization tool for V-Morph.

This script provides utilities to:
1. Convert FP32 ONNX models to FP16
2. Quantize FP32 models to INT8 (dynamic/static)
3. Optimize ONNX models (graph fusion, constant folding, etc.)
4. Generate TensorRT engines from ONNX models
"""

import os
import sys
import argparse
import logging
from pathlib import Path

try:
    import onnx
    import onnxruntime as ort
    from onnxruntime.quantization import quantize_dynamic, quantize_static, QuantType
    from onnxruntime.quantization.calibrate import CalibrationDataReader
    import numpy as np
except ImportError as e:
    print(f"Missing dependencies: {e}")
    print("Install with: pip install onnx onnxruntime onnxruntime-tools")
    sys.exit(1)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def optimize_onnx_model(input_path, output_path, optimization_level="all"):
    """
    Optimize ONNX model using ONNX Runtime's optimizer.
    
    Args:
        input_path: Path to input ONNX model
        output_path: Path to save optimized model
        optimization_level: "basic", "extended", or "all"
    """
    logger.info(f"Optimizing model: {input_path} -> {output_path}")
    
    sess_options = ort.SessionOptions()
    sess_options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    sess_options.optimized_model_filepath = output_path
    
    # Create session to trigger optimization
    _ = ort.InferenceSession(input_path, sess_options)
    
    logger.info(f"Optimized model saved to: {output_path}")
    return True

def convert_fp32_to_fp16(input_path, output_path):
    """
    Convert FP32 ONNX model to FP16 using ONNX's float16 converter.
    """
    logger.info(f"Converting FP32 to FP16: {input_path} -> {output_path}")
    
    try:
        from onnxconverter_common import float16
        
        model = onnx.load(input_path)
        model_fp16 = float16.convert_float_to_float16(
            model,
            keep_io_types=True,  # Keep input/output as FP32 for compatibility
            node_block_list=None,
            op_block_list=None
        )
        
        onnx.save(model_fp16, output_path)
        logger.info(f"FP16 model saved to: {output_path}")
        return True
    except ImportError:
        logger.error("onnxconverter_common not installed. Install with: pip install onnxconverter_common")
        return False
    except Exception as e:
        logger.error(f"FP16 conversion failed: {e}")
        return False

def quantize_model_dynamic(input_path, output_path, weight_type=QuantType.QInt8):
    """
    Dynamic quantization of ONNX model (weights only, activations remain FP32).
    """
    logger.info(f"Dynamic quantization: {input_path} -> {output_path}")
    
    try:
        quantize_dynamic(
            model_input=input_path,
            model_output=output_path,
            weight_type=weight_type,
            optimize_model=True,
            per_channel=True,
            reduce_range=False
        )
        logger.info(f"Quantized model saved to: {output_path}")
        return True
    except Exception as e:
        logger.error(f"Dynamic quantization failed: {e}")
        return False

class CalibrationReader(CalibrationDataReader):
    """Calibration data reader for static quantization."""
    
    def __init__(self, calibration_data_path, input_name):
        self.calibration_data_path = calibration_data_path
        self.input_name = input_name
        self.data = None
        self.index = 0
        self._load_data()
    
    def _load_data(self):
        if self.calibration_data_path.endswith('.npy'):
            self.data = np.load(self.calibration_data_path)
        elif self.calibration_data_path.endswith('.npz'):
            npz = np.load(self.calibration_data_path)
            self.data = npz[self.input_name] if self.input_name in npz else list(npz.values())[0]
        else:
            raise ValueError(f"Unsupported calibration data format: {self.calibration_data_path}")
        
        # Ensure data is in correct shape [N, ...]
        if self.data.ndim == 1:
            self.data = self.data.reshape(1, -1)
    
    def get_next(self):
        if self.index >= len(self.data):
            return None
        
        sample = self.data[self.index:self.index+1].astype(np.float32)
        self.index += 1
        return {self.input_name: sample}
    
    def rewind(self):
        self.index = 0

def quantize_model_static(input_path, output_path, calibration_data_path, 
                          input_name="input", weight_type=QuantType.QInt8,
                          activation_type=QuantType.QInt8):
    """
    Static quantization of ONNX model (weights and activations quantized).
    Requires calibration data.
    """
    logger.info(f"Static quantization: {input_path} -> {output_path}")
    
    try:
        reader = CalibrationReader(calibration_data_path, input_name)
        
        quantize_static(
            model_input=input_path,
            model_output=output_path,
            calibration_data_reader=reader,
            quant_format=ort.quantization.QuantFormat.QDQ,
            activation_type=activation_type,
            weight_type=weight_type,
            optimize_model=True,
            per_channel=True,
            reduce_range=False
        )
        logger.info(f"Static quantized model saved to: {output_path}")
        return True
    except Exception as e:
        logger.error(f"Static quantization failed: {e}")
        return False

def build_tensorrt_engine(onnx_path, engine_path, precision="fp16", 
                          workspace_size=1<<30, batch_size=1):
    """
    Build TensorRT engine from ONNX model using ONNX Runtime's TensorRT EP.
    Note: This creates the engine cache, not a standalone .engine file.
    For standalone engine, use trtexec from TensorRT directly.
    """
    logger.info(f"Building TensorRT engine cache for: {onnx_path}")
    
    try:
        sess_options = ort.SessionOptions()
        sess_options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        
        # TensorRT provider options
        trt_options = {
            'device_id': 0,
            'trt_max_workspace_size': workspace_size,
            'trt_fp16_enable': 1 if precision == 'fp16' else 0,
            'trt_int8_enable': 1 if precision == 'int8' else 0,
            'trt_engine_cache_enable': 1,
            'trt_engine_cache_path': os.path.dirname(engine_path) or '.',
            'trt_min_timing_iterations': 2,
            'trt_avg_timing_iterations': 1,
        }
        
        providers = [
            ('TensorrtExecutionProvider', trt_options),
            'CUDAExecutionProvider',
            'CPUExecutionProvider'
        ]
        
        # Create session to build engine cache
        session = ort.InferenceSession(onnx_path, sess_options, providers=providers)
        
        # Run dummy inference to trigger engine building
        input_info = session.get_inputs()[0]
        dummy_input = np.random.randn(*input_info.shape).astype(np.float32)
        _ = session.run(None, {input_info.name: dummy_input})
        
        logger.info(f"TensorRT engine cache built at: {trt_options['trt_engine_cache_path']}")
        return True
    except Exception as e:
        logger.error(f"TensorRT engine build failed: {e}")
        return False

def validate_model(model_path):
    """Validate ONNX model and print info."""
    logger.info(f"Validating model: {model_path}")
    
    try:
        model = onnx.load(model_path)
        onnx.checker.check_model(model)
        
        logger.info(f"Model: {model_path}")
        logger.info(f"  IR version: {model.ir_version}")
        logger.info(f"  Producer: {model.producer_name} {model.producer_version}")
        logger.info(f"  Domain: {model.domain}")
        logger.info(f"  Model version: {model.model_version}")
        logger.info(f"  Doc string: {model.doc_string}")
        logger.info(f"  Opset imports: {[(op.domain, op.version) for op in model.opset_import]}")
        
        # Print graph info
        graph = model.graph
        logger.info(f"  Inputs: {len(graph.input)}")
        for inp in graph.input:
            shape = [d.dim_value if d.dim_value > 0 else -1 for d in inp.type.tensor_type.shape.dim]
            logger.info(f"    {inp.name}: {inp.type.tensor_type.elem_type} {shape}")
        
        logger.info(f"  Outputs: {len(graph.output)}")
        for out in graph.output:
            shape = [d.dim_value if d.dim_value > 0 else -1 for d in out.type.tensor_type.shape.dim]
            logger.info(f"    {out.name}: {out.type.tensor_type.elem_type} {shape}")
        
        logger.info(f"  Nodes: {len(graph.node)}")
        op_types = {}
        for node in graph.node:
            op_types[node.op_type] = op_types.get(node.op_type, 0) + 1
        for op, count in sorted(op_types.items()):
            logger.info(f"    {op}: {count}")
        
        return True
    except Exception as e:
        logger.error(f"Model validation failed: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='V-Morph Model Optimization Tool')
    subparsers = parser.add_subparsers(dest='command', help='Command')
    
    # Optimize command
    opt_parser = subparsers.add_parser('optimize', help='Optimize ONNX model')
    opt_parser.add_argument('input', help='Input ONNX model')
    opt_parser.add_argument('output', help='Output optimized model')
    opt_parser.add_argument('--level', default='all', choices=['basic', 'extended', 'all'],
                           help='Optimization level')
    
    # FP16 conversion
    fp16_parser = subparsers.add_parser('fp16', help='Convert FP32 to FP16')
    fp16_parser.add_argument('input', help='Input ONNX model')
    fp16_parser.add_argument('output', help='Output FP16 model')
    
    # Dynamic quantization
    dyn_parser = subparsers.add_parser('quantize-dynamic', help='Dynamic quantization (INT8 weights)')
    dyn_parser.add_argument('input', help='Input ONNX model')
    dyn_parser.add_argument('output', help='Output quantized model')
    dyn_parser.add_argument('--weight-type', default='qint8', choices=['qint8', 'quint8'],
                           help='Weight quantization type')
    
    # Static quantization
    stat_parser = subparsers.add_parser('quantize-static', help='Static quantization (INT8 weights + activations)')
    stat_parser.add_argument('input', help='Input ONNX model')
    stat_parser.add_argument('output', help='Output quantized model')
    stat_parser.add_argument('calibration_data', help='Calibration data (.npy or .npz)')
    stat_parser.add_argument('--input-name', default='input', help='Input tensor name')
    stat_parser.add_argument('--weight-type', default='qint8', choices=['qint8', 'quint8'])
    stat_parser.add_argument('--activation-type', default='qint8', choices=['qint8', 'quint8'])
    
    # TensorRT engine
    trt_parser = subparsers.add_parser('tensorrt', help='Build TensorRT engine cache')
    trt_parser.add_argument('input', help='Input ONNX model')
    trt_parser.add_argument('output', help='Output directory for engine cache')
    trt_parser.add_argument('--precision', default='fp16', choices=['fp32', 'fp16', 'int8'])
    trt_parser.add_argument('--workspace', type=int, default=1<<30, help='Workspace size in bytes')
    
    # Validate
    val_parser = subparsers.add_parser('validate', help='Validate ONNX model')
    val_parser.add_argument('input', help='Input ONNX model')
    
    args = parser.parse_args()
    
    if args.command == 'optimize':
        optimize_onnx_model(args.input, args.output, args.level)
    elif args.command == 'fp16':
        convert_fp32_to_fp16(args.input, args.output)
    elif args.command == 'quantize-dynamic':
        weight_type = QuantType.QInt8 if args.weight_type == 'qint8' else QuantType.QUInt8
        quantize_model_dynamic(args.input, args.output, weight_type)
    elif args.command == 'quantize-static':
        weight_type = QuantType.QInt8 if args.weight_type == 'qint8' else QuantType.QUInt8
        activation_type = QuantType.QInt8 if args.activation_type == 'qint8' else QuantType.QUInt8
        quantize_model_static(args.input, args.output, args.calibration_data,
                             args.input_name, weight_type, activation_type)
    elif args.command == 'tensorrt':
        build_tensorrt_engine(args.input, args.output, args.precision, args.workspace)
    elif args.command == 'validate':
        validate_model(args.input)
    else:
        parser.print_help()
        sys.exit(1)

if __name__ == '__main__':
    main()