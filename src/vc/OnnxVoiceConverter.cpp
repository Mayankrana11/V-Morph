#include "OnnxVoiceConverter.h"
#include "inference/OnnxInferenceEngine.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace rtvcc {

struct OnnxVoiceConverter::Impl {
    Impl() = default;
    ~Impl() = default;
};

OnnxVoiceConverter::OnnxVoiceConverter() : pimpl_(std::make_unique<Impl>()) {}
OnnxVoiceConverter::~OnnxVoiceConverter() = default;

bool OnnxVoiceConverter::initialize(const VoiceConverterConfig& config) {
    config_ = config;
    last_error_.clear();

    // Parse model path and validate
    if (config.model_path.empty()) {
        last_error_ = "Model path not specified";
        return false;
    }

    if (!std::filesystem::exists(config.model_path)) {
        last_error_ = "Model file not found: " + config.model_path;
        return false;
    }

    // Set sample rates
    input_sample_rate_ = config.sample_rate > 0 ? config.sample_rate : 48000;
    output_sample_rate_ = config.sample_rate > 0 ? config.sample_rate : 48000;

    // Create inference engine
    inference_engine_ = createInferenceEngine("onnx");
    if (!inference_engine_) {
        last_error_ = "Failed to create ONNX inference engine";
        return false;
    }

    InferenceConfig infer_config;
    infer_config.model_path = config.model_path;
    infer_config.execution_provider = config.execution_provider.empty() ? "CPU" : config.execution_provider;
    infer_config.intra_op_threads = 1;
    infer_config.inter_op_threads = 1;
    infer_config.graph_optimization = true;

    if (!inference_engine_->initialize(infer_config)) {
        last_error_ = "Failed to initialize inference engine: " + inference_engine_->getLastError();
        return false;
    }

    // Get model manifest
    auto manifest = inference_engine_->getManifest();
    model_sample_rate_ = manifest.sample_rate > 0 ? manifest.sample_rate : 16000;

    // Setup resamplers if needed
    if (input_sample_rate_ != model_sample_rate_) {
        input_resampler_ = std::make_unique<Resampler>();
        if (!input_resampler_->configure(input_sample_rate_, model_sample_rate_, 1)) {
            last_error_ = "Failed to configure input resampler";
            return false;
        }
    }

    if (output_sample_rate_ != model_sample_rate_) {
        output_resampler_ = std::make_unique<Resampler>();
        if (!output_resampler_->configure(model_sample_rate_, output_sample_rate_, 1)) {
            last_error_ = "Failed to configure output resampler";
            return false;
        }
    }

    // Calculate chunk size in frames at model sample rate
    chunk_frames_ = (model_sample_rate_ * config.chunk_size_ms) / 1000;
    if (chunk_frames_ == 0) chunk_frames_ = 320; // 20ms at 16kHz

    // Get algorithmic latency from model manifest
    algorithmic_latency_samples_ = manifest.lookahead_ms * model_sample_rate_ / 1000;
    algorithmic_latency_ms_ = manifest.lookahead_ms;

    // Get input/output tensor info
    auto input_infos = inference_engine_->getInputInfos();
    auto output_infos = inference_engine_->getOutputInfos();

    // Prepare inference buffers
    size_t max_input_elements = 0;
    for (const auto& info : input_infos) {
        max_input_elements = std::max(max_input_elements, info.element_count());
    }
    size_t max_output_elements = 0;
    for (const auto& info : output_infos) {
        max_output_elements = std::max(max_output_elements, info.element_count());
    }

    model_input_.resize(max_input_elements);
    model_output_.resize(max_output_elements);
    resampled_input_.resize(chunk_frames_);
    resampled_output_.resize(chunk_frames_);

    // Prepare inference I/O arrays
    inference_inputs_.resize(input_infos.size());
    inference_outputs_.resize(output_infos.size());

    // Warm up the model
    if (!inference_engine_->warmup()) {
        last_error_ = "Model warmup failed: " + inference_engine_->getLastError();
        return false;
    }

    first_chunk_ = true;
    initialized_ = true;
    return true;
}

OnnxVoiceConverter::ProcessResult OnnxVoiceConverter::process(const float* input, size_t frames, float* output) {
    ProcessResult result;
    result.status = ProcessResult::Status::Success;
    result.input_consumed = 0;
    result.output_produced = 0;

    if (!initialized_) {
        result.status = ProcessResult::Status::NotInitialized;
        result.error = "Converter not initialized";
        return result;
    }

    if (!input || !output) {
        result.status = ProcessResult::Status::Error;
        result.error = "Null pointer";
        return result;
    }

    if (frames == 0) {
        return result;
    }

    // Resample input to model sample rate if needed
    const float* model_input_ptr = input;
    size_t model_frames = frames;

    if (input_resampler_) {
        size_t out_frames = input_resampler_->process(input, frames, resampled_input_.data(), resampled_input_.size());
        model_input_ptr = resampled_input_.data();
        model_frames = out_frames;
    }

    // Process in chunks matching model's expected chunk size
    size_t total_output_frames = 0;
    size_t input_consumed = 0;

    for (size_t offset = 0; offset < model_frames; ) {
        size_t chunk_size = std::min(chunk_frames_, model_frames - offset);
        
        // Prepare model input
        const float* chunk_input = model_input_ptr + offset;
        if (chunk_size < chunk_frames_) {
            // Pad last chunk with zeros
            std::copy(chunk_input, chunk_input + chunk_size, model_input_.data());
            std::fill(model_input_.data() + chunk_size, model_input_.data() + chunk_frames_, 0.0f);
            chunk_input = model_input_.data();
        } else {
            chunk_input = chunk_input;
        }

        // Set up inference inputs (handle stateful models)
        inference_inputs_[0] = chunk_input;
        
        // For stateful models, additional state inputs would be set here
        for (size_t i = 1; i < inference_inputs_.size(); ++i) {
            // State inputs would come from previous inference
            inference_inputs_[i] = nullptr; // Will be handled by model's internal state
        }

        // Run inference
        inference_outputs_[0] = resampled_output_.data();
        for (size_t i = 1; i < inference_outputs_.size(); ++i) {
            inference_outputs_[i] = nullptr;
        }

        auto infer_result = inference_engine_->run(inference_inputs_, inference_outputs_);
        if (!infer_result.success) {
            result.status = ProcessResult::Status::Error;
            result.error = "Inference failed: " + infer_result.error;
            return result;
        }

        // Resample output back to original sample rate if needed
        const float* out_ptr = resampled_output_.data();
        size_t out_frames = chunk_frames_;

        if (output_resampler_) {
            out_frames = output_resampler_->process(resampled_output_.data(), chunk_frames_, 
                                                    output + total_output_frames, 
                                                    frames - total_output_frames);
        } else {
            size_t copy_frames = std::min(chunk_frames_, frames - total_output_frames);
            std::copy(resampled_output_.data(), resampled_output_.data() + copy_frames, 
                      output + total_output_frames);
            out_frames = copy_frames;
        }

        total_output_frames += out_frames;
        input_consumed += chunk_size;
        offset += chunk_size;
    }

    // Apply mix at output sample rate
    if (mix_ < 1.0f && mix_ > 0.0f) {
        for (size_t i = 0; i < total_output_frames; ++i) {
            output[i] = output[i] * mix_;  // Wet only scaled by mix
        }
    } else if (mix_ <= 0.0f) {
        // Dry only - output already contains dry signal from input if passthrough
        // For true dry, we'd need to copy input to output, but that's handled by the caller
        std::fill(output, output + total_output_frames, 0.0f);
    }

    result.input_consumed = frames;
    result.output_produced = total_output_frames;
    return result;
}

OnnxVoiceConverter::ProcessResult OnnxVoiceConverter::processPlanar(const float* const* input, size_t frames, float* const* output) {
    // For mono, just delegate to interleaved version
    if (input[0] && output[0]) {
        return process(input[0], frames, output[0]);
    }
    
    ProcessResult result;
    result.status = ProcessResult::Status::Error;
    result.error = "Null pointer in planar process";
    return result;
}

void OnnxVoiceConverter::reset() {
    if (inference_engine_) {
        inference_engine_->reset();
    }
    if (input_resampler_) {
        input_resampler_->reset();
    }
    if (output_resampler_) {
        output_resampler_->reset();
    }
    first_chunk_ = true;
}

OnnxVoiceConverter::ProcessResult OnnxVoiceConverter::flush(float* output, size_t max_frames) {
    ProcessResult result;
    result.status = ProcessResult::Status::Success;
    result.input_consumed = 0;
    result.output_produced = 0;
    
    if (output && max_frames > 0) {
        std::fill(output, output + max_frames, 0.0f);
    }
    return result;
}

bool OnnxVoiceConverter::setTargetVoice(const std::string& voice_id) {
    // Could be implemented by feeding speaker embedding as additional input
    // For now, just store the voice ID
    config_.target_voice = voice_id;
    return true;
}

bool OnnxVoiceConverter::setPitchShift(float semitones) {
    config_.pitch_shift = semitones;
    // Would need model support for pitch shifting
    return true;
}

bool OnnxVoiceConverter::setFormantShift(float factor) {
    config_.formant_shift = factor;
    // Would need model support for formant shifting
    return true;
}

} // namespace rtvcc