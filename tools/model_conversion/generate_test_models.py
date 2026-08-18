#!/usr/bin/env python3
"""
Generate a simple test ONNX model for V-Morph validation.

This creates a simple identity/passthrough model that can be used to test
the ONNX Runtime integration without requiring a real voice conversion model.

The model takes a float32 tensor of shape [1, 1, chunk_size] and outputs
the same tensor (identity), simulating a passthrough voice converter.
"""

import os
import sys
import numpy as np

try:
    import onnx
    import onnx.helper as helper
    import onnx.numpy_helper as numpy_helper
    from onnx import TensorProto
except ImportError:
    print("ONNX not installed. Install with: pip install onnx onnxruntime")
    sys.exit(1)

def create_identity_model(chunk_size=320, sample_rate=16000):
    """Create a simple identity ONNX model."""
    
    # Input: [batch=1, channels=1, samples=chunk_size]
    input_shape = [1, 1, chunk_size]
    output_shape = [1, 1, chunk_size]
    
    # Create input/output value info
    input_info = helper.make_tensor_value_info(
        'input', TensorProto.FLOAT, input_shape)
    output_info = helper.make_tensor_value_info(
        'output', TensorProto.FLOAT, output_shape)
    
    # Create identity node
    identity_node = helper.make_node(
        'Identity',
        inputs=['input'],
        outputs=['output'],
        name='identity'
    )
    
    # Create graph
    graph = helper.make_graph(
        nodes=[identity_node],
        name='IdentityGraph',
        inputs=[input_info],
        outputs=[output_info],
        initializer=[]
    )
    
    # Create model
    model = helper.make_model(
        graph,
        producer_name='V-Morph_TestModelGenerator',
        opset_imports=[helper.make_opsetid('', 17)],
        metadata_props=[
            helper.make_metadata_prop('sample_rate', str(sample_rate)),
            helper.make_metadata_prop('channels', '1'),
            helper.make_metadata_prop('chunk_size', str(chunk_size)),
            helper.make_metadata_prop('streaming', 'true'),
            helper.make_metadata_prop('lookahead_ms', '0'),
        ]
    )
    
    # Set model version
    model.model_version = 1
    model.doc_string = "Test identity model for V-Morph ONNX integration validation"
    
    return model

def create_simple_gain_model(chunk_size=320, sample_rate=16000, gain_db=0.0):
    """Create a simple gain model for testing."""
    
    gain_linear = 10 ** (gain_db / 20.0)
    
    input_shape = [1, 1, chunk_size]
    output_shape = [1, 1, chunk_size]
    
    input_info = helper.make_tensor_value_info(
        'input', TensorProto.FLOAT, input_shape)
    output_info = helper.make_tensor_value_info(
        'output', TensorProto.FLOAT, output_shape)
    
    # Create constant for gain
    gain_tensor = numpy_helper.from_array(
        np.array([gain_linear], dtype=np.float32), name='gain_const')
    
    # Mul node: output = input * gain
    mul_node = helper.make_node(
        'Mul',
        inputs=['input', 'gain_const'],
        outputs=['output'],
        name='gain_mul'
    )
    
    graph = helper.make_graph(
        nodes=[mul_node],
        name='GainGraph',
        inputs=[input_info],
        outputs=[output_info],
        initializer=[gain_tensor]
    )
    
    model = helper.make_model(
        graph,
        producer_name='V-Morph_TestModelGenerator',
        opset_imports=[helper.make_opsetid('', 17)],
        metadata_props=[
            helper.make_metadata_prop('sample_rate', str(sample_rate)),
            helper.make_metadata_prop('channels', '1'),
            helper.make_metadata_prop('chunk_size', str(chunk_size)),
            helper.make_metadata_prop('gain_db', str(gain_db)),
        ]
    )
    
    model.model_version = 1
    model.doc_string = f"Test gain model ({gain_db}dB) for V-Morph ONNX integration validation"
    
    return model

def create_stateful_model(chunk_size=320, sample_rate=16000, state_size=128):
    """Create a simple stateful model (simulating RNN/GRU) for testing streaming."""
    
    input_shape = [1, 1, chunk_size]
    state_shape = [1, 1, state_size]
    output_shape = [1, 1, chunk_size]
    
    input_info = helper.make_tensor_value_info('input', TensorProto.FLOAT, input_shape)
    state_in_info = helper.make_tensor_value_info('state_in', TensorProto.FLOAT, state_shape)
    output_info = helper.make_tensor_value_info('output', TensorProto.FLOAT, output_shape)
    state_out_info = helper.make_tensor_value_info('state_out', TensorProto.FLOAT, state_shape)
    
    # Simple stateful processing: output = input + state, state_out = state_in * 0.9
    # This simulates a simple recurrent connection
    
    # Create constants
    decay_tensor = numpy_helper.from_array(
        np.array([0.9], dtype=np.float32), name='decay')
    one_tensor = numpy_helper.from_array(
        np.array([1.0], dtype=np.float32), name='one')
    
    # state_decayed = state_in * decay
    state_decayed_name = 'state_decayed'
    state_decayed_node = helper.make_node(
        'Mul',
        inputs=['state_in', 'decay'],
        outputs=[state_decayed_name],
        name='state_decay'
    )
    
    # output = input + state_decayed
    add_node = helper.make_node(
        'Add',
        inputs=['input', state_decayed_name],
        outputs=['output'],
        name='output_add'
    )
    
    # state_out = state_decayed (pass through)
    state_out_node = helper.make_node(
        'Identity',
        inputs=[state_decayed_name],
        outputs=['state_out'],
        name='state_out_identity'
    )
    
    graph = helper.make_graph(
        nodes=[state_decayed_node, add_node, state_out_node],
        name='StatefulGraph',
        inputs=[input_info, state_in_info],
        outputs=[output_info, state_out_info],
        initializer=[decay_tensor, one_tensor]
    )
    
    model = helper.make_model(
        graph,
        producer_name='V-Morph_TestModelGenerator',
        opset_imports=[helper.make_opsetid('', 17)],
        metadata_props=[
            helper.make_metadata_prop('sample_rate', str(sample_rate)),
            helper.make_metadata_prop('channels', '1'),
            helper.make_metadata_prop('chunk_size', str(chunk_size)),
            helper.make_metadata_prop('state_size', str(state_size)),
            helper.make_metadata_prop('streaming', 'true'),
            helper.make_metadata_prop('lookahead_ms', '0'),
        ]
    )
    
    model.model_version = 1
    model.doc_string = f"Test stateful model (state_size={state_size}) for V-Morph streaming validation"
    
    return model

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Generate test ONNX models for V-Morph')
    parser.add_argument('--output-dir', default='models', help='Output directory')
    parser.add_argument('--chunk-size', type=int, default=320, help='Chunk size in samples')
    parser.add_argument('--sample-rate', type=int, default=16000, help='Sample rate')
    parser.add_argument('--state-size', type=int, default=128, help='State size for stateful model')
    
    args = parser.parse_args()
    
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Generate identity model
    identity_model = create_identity_model(args.chunk_size, args.sample_rate)
    identity_path = os.path.join(args.output_dir, 'test_identity.onnx')
    onnx.save(identity_model, identity_path)
    print(f"Created: {identity_path}")
    
    # Generate gain model (6dB)
    gain_model = create_simple_gain_model(args.chunk_size, args.sample_rate, 6.0)
    gain_path = os.path.join(args.output_dir, 'test_gain_6db.onnx')
    onnx.save(gain_model, gain_path)
    print(f"Created: {gain_path}")
    
    # Generate stateful model
    stateful_model = create_stateful_model(args.chunk_size, args.sample_rate, args.state_size)
    stateful_path = os.path.join(args.output_dir, 'test_stateful.onnx')
    onnx.save(stateful_model, stateful_path)
    print(f"Created: {stateful_path}")
    
    print("\nTest models generated successfully!")
    print(f"Use with: v-morph.exe --model {identity_path} --converter onnx")

if __name__ == '__main__':
    main()