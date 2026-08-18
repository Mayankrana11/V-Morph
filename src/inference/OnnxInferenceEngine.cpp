#include "OnnxInferenceEngine.h"
#include "TensorUtils.h"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <cstdint>

namespace rtvcc {

struct OnnxInferenceEngine::Impl {
    Impl() : env_(ORT_LOGGING_LEVEL_WARNING, "RTVC_OnnxInferenceEngine") {}

    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::RunOptions> run_options_;

    InferenceConfig config_;
    ModelManifest manifest_;
    std::vector<TensorInfo> input_infos_;
    std::vector<TensorInfo> output_infos_;

    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<const char*> input_names_c_;
    std::vector<const char*> output_names_c_;

    // Pre-allocated tensors for zero-copy inference
    std::vector<Ort::Value> input_tensors_;
    std::vector<Ort::Value> output_tensors_;
    Ort::MemoryInfo memory_info_ = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // Streaming state
    std::vector<std::vector<float>> state_buffers_;
    bool has_state_ = false;

    std::string last_error_;
    bool initialized_ = false;

    void setError(const std::string& err) {
        last_error_ = err;
    }

    bool loadModel() {
        try {
            // Configure session options
            session_options_.SetIntraOpNumThreads(config_.intra_op_threads);
            session_options_.SetInterOpNumThreads(config_.inter_op_threads);
            session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            if (config_.enable_profiling) {
                session_options_.EnableProfiling("onnx_profile");
            }

            // Select execution provider
            if (config_.execution_provider == "CUDA") {
                OrtCUDAProviderOptions cuda_options;
                session_options_.AppendExecutionProvider_CUDA(cuda_options);
            } else if (config_.execution_provider == "TensorRT") {
                OrtTensorRTProviderOptions trt_options;
                session_options_.AppendExecutionProvider_TensorRT(trt_options);
            } else if (config_.execution_provider == "DirectML") {
                OrtDirectMLProviderOptions dml_options;
                session_options_.AppendExecutionProvider_DML(dml_options);
            }
            // Default: CPU execution provider

            // Load model
            session_ = std::make_unique<Ort::Session>(env_, config_.model_path.c_str(), session_options_);

            // Get model metadata
            Ort::ModelMetadata metadata = session_->GetModelMetadata();
            manifest_.name = metadata.GetName();
            manifest_.version = metadata.GetVersion();
            manifest_.description = metadata.GetDescription();

            // Get input/output info
            size_t num_inputs = session_->GetInputCount();
            size_t num_outputs = session_->GetOutputCount();

            input_infos_.clear();
            output_infos_.clear();
            input_names_.clear();
            output_names_.clear();
            input_names_c_.clear();
            output_names_c_.clear();

            Ort::AllocatorWithDefaultOptions allocator;

            for (size_t i = 0; i < num_inputs; ++i) {
                std::string name = session_->GetInputNameAllocated(i, allocator).get();
                input_names_.push_back(name);
                input_names_c_.push_back(input_names_.back().c_str());

                Ort::TypeInfo type_info = session_->GetInputTypeInfo(i);
                auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
                
                TensorInfo info;
                info.name = name;
                info.shape = tensor_info.GetShape();
                info.type = getOrtTypeString(tensor_info.GetElementType());
                input_infos_.push_back(info);
            }

            for (size_t i = 0; i < num_outputs; ++i) {
                std::string name = session_->GetOutputNameAllocated(i, allocator).get();
                output_names_.push_back(name);
                output_names_c_.push_back(output_names_.back().c_str());

                Ort::TypeInfo type_info = session_->GetOutputTypeInfo(i);
                auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
                
                TensorInfo info;
                info.name = name;
                info.shape = tensor_info.GetShape();
                info.type = getOrtTypeString(tensor_info.GetElementType());
                output_infos_.push_back(info);
            }

            // Set manifest from model info
            manifest_.format = "onnx";
            if (!input_infos_.empty()) {
                manifest_.input_shape = input_infos_[0].shape;
                manifest_.channels = 1;
                for (int64_t dim : input_infos_[0].shape) {
                    if (dim > 0 && manifest_.channels == 1) manifest_.channels = static_cast<int>(dim);
                }
            }
            if (!output_infos_.empty()) {
                manifest_.output_shape = output_infos_[0].shape;
            }

            // Check if model has state (stateful/streaming)
            has_state_ = false;
            for (const auto& input : input_infos_) {
                if (input.name.find("state") != std::string::npos || 
                    input.name.find("hidden") != std::string::npos ||
                    input.name.find("cell") != std::string::npos) {
                    has_state_ = true;
                    break;
                }
            }

            // Pre-allocate input tensors
            input_tensors_.resize(num_inputs);
            for (size_t i = 0; i < num_inputs; ++i) {
                size_t elements = 1;
                for (int64_t dim : input_infos_[i].shape) {
                    if (dim > 0) elements *= static_cast<size_t>(dim);
                }
                // Will be resized per inference call
            }

            // Pre-allocate output tensors
            output_tensors_.resize(num_outputs);

            run_options_ = std::make_unique<Ort::RunOptions>();
            if (config_.enable_profiling) {
                run_options_->SetRunTag("RTVC_Inference");
            }

            initialized_ = true;
            return true;
        } catch (const Ort::Exception& e) {
            setError(std::string("ONNX Runtime error: ") + e.what());
            return false;
        } catch (const std::exception& e) {
            setError(std::string("Error: ") + e.what());
            return false;
        }
    }

    std::string getOrtTypeString(ONNXTensorElementDataType type) {
        switch (type) {
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return "float32";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: return "float64";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: return "int8";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: return "int16";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return "int32";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return "int64";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return "uint8";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: return "uint16";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: return "uint32";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: return "uint64";
            case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return "bool";
            default: return "unknown";
        }
    }

    bool validateInputShapes(const std::vector<const float*>& inputs) {
        if (inputs.size() != input_infos_.size()) {
            setError("Input count mismatch: expected " + std::to_string(input_infos_.size()) + 
                     ", got " + std::to_string(inputs.size()));
            return false;
        }
        return true;
    }

    void allocateInputTensors(const std::vector<const float*>& inputs) {
        input_tensors_.clear();
        input_tensors_.reserve(inputs.size());

        for (size_t i = 0; i < inputs.size(); ++i) {
            const auto& info = input_infos_[i];
            size_t elements = info.element_count();
            
            // Handle dynamic batch/sequence dimensions (-1)
            std::vector<int64_t> shape = info.shape;
            for (auto& dim : shape) {
                if (dim == -1) dim = 1; // Default to 1 for dynamic dims
            }
            
            input_tensors_.push_back(Ort::Value::CreateTensor<float>(
                memory_info_, 
                const_cast<float*>(inputs[i]), 
                elements, 
                shape.data(), 
                shape.size()
            ));
        }
    }

    void allocateOutputTensors(std::vector<float*>& outputs) {
        output_tensors_.clear();
        output_tensors_.reserve(outputs.size());

        for (size_t i = 0; i < outputs.size(); ++i) {
            const auto& info = output_infos_[i];
            size_t elements = info.element_count();
            
            std::vector<int64_t> shape = info.shape;
            for (auto& dim : shape) {
                if (dim == -1) dim = 1;
            }
            
            output_tensors_.push_back(Ort::Value::CreateTensor<float>(
                memory_info_, 
                outputs[i], 
                elements, 
                shape.data(), 
                shape.size()
            ));
        }
    }

    void allocateNamedInputTensors(const std::vector<std::pair<std::string, const float*>>& inputs) {
        input_tensors_.clear();
        input_tensors_.reserve(inputs.size());

        std::unordered_map<std::string, size_t> input_name_to_idx;
        for (size_t i = 0; i < input_names_.size(); ++i) {
            input_name_to_idx[input_names_[i]] = i;
        }

        // Sort inputs by model's expected order
        std::vector<std::pair<std::string, const float*>> sorted_inputs = inputs;
        std::sort(sorted_inputs.begin(), sorted_inputs.end(),
                  [&](const auto& a, const auto& b) {
                      return input_name_to_idx[a.first] < input_name_to_idx[b.first];
                  });

        for (const auto& [name, data] : sorted_inputs) {
            auto it = input_name_to_idx.find(name);
            if (it == input_name_to_idx.end()) {
                setError("Unknown input name: " + name);
                return;
            }
            size_t idx = it->second;
            const auto& info = input_infos_[idx];
            size_t elements = info.element_count();
            
            std::vector<int64_t> shape = info.shape;
            for (auto& dim : shape) {
                if (dim == -1) dim = 1;
            }
            
            input_tensors_.push_back(Ort::Value::CreateTensor<float>(
                memory_info_, 
                const_cast<float*>(data), 
                elements, 
                shape.data(), 
                shape.size()
            ));
        }
    }

    void allocateNamedOutputTensors(std::vector<std::pair<std::string, float*>>& outputs) {
        output_tensors_.clear();
        output_tensors_.reserve(outputs.size());

        std::unordered_map<std::string, size_t> output_name_to_idx;
        for (size_t i = 0; i < output_names_.size(); ++i) {
            output_name_to_idx[output_names_[i]] = i;
        }

        std::vector<std::pair<std::string, float*>> sorted_outputs = outputs;
        std::sort(sorted_outputs.begin(), sorted_outputs.end(),
                  [&](const auto& a, const auto& b) {
                      return output_name_to_idx[a.first] < output_name_to_idx[b.first];
                  });

        for (const auto& [name, data] : sorted_outputs) {
            auto it = output_name_to_idx.find(name);
            if (it == output_name_to_idx.end()) {
                setError("Unknown output name: " + name);
                return;
            }
            size_t idx = it->second;
            const auto& info = output_infos_[idx];
            size_t elements = info.element_count();
            
            std::vector<int64_t> shape = info.shape;
            for (auto& dim : shape) {
                if (dim == -1) dim = 1;
            }
            
            output_tensors_.push_back(Ort::Value::CreateTensor<float>(
                memory_info_, 
                data, 
                elements, 
                shape.data(), 
                shape.size()
            ));
        }
    }
};

OnnxInferenceEngine::OnnxInferenceEngine() : pimpl_(std::make_unique<Impl>()) {}
OnnxInferenceEngine::~OnnxInferenceEngine() = default;

bool OnnxInferenceEngine::initialize(const InferenceConfig& config) {
    pimpl_->config_ = config;
    pimpl_->last_error_.clear();
    pimpl_->initialized_ = false;

    if (!std::filesystem::exists(config.model_path)) {
        pimpl_->setError("Model file not found: " + config.model_path);
        return false;
    }

    return pimpl_->loadModel();
}

ModelManifest OnnxInferenceEngine::getManifest() const {
    return pimpl_->manifest_;
}

std::vector<TensorInfo> OnnxInferenceEngine::getInputInfos() const {
    return pimpl_->input_infos_;
}

std::vector<TensorInfo> OnnxInferenceEngine::getOutputInfos() const {
    return pimpl_->output_infos_;
}

InferenceResult OnnxInferenceEngine::run(const std::vector<const float*>& inputs,
                                         std::vector<float*>& outputs) {
    InferenceResult result;
    result.success = false;

    if (!pimpl_->initialized_) {
        result.error = "Engine not initialized";
        return result;
    }

    if (!pimpl_->validateInputShapes(inputs)) {
        result.error = pimpl_->last_error_;
        return result;
    }

    if (inputs.size() != pimpl_->input_infos_.size() || outputs.size() != pimpl_->output_infos_.size()) {
        result.error = "Input/output count mismatch";
        return result;
    }

    auto start = std::chrono::high_resolution_clock::now();

    try {
        pimpl_->allocateInputTensors(inputs);
        pimpl_->allocateOutputTensors(outputs);

        pimpl_->session_->Run(*pimpl_->run_options_,
                              pimpl_->input_names_c_.data(),
                              pimpl_->input_tensors_.data(),
                              pimpl_->input_tensors_.size(),
                              pimpl_->output_names_c_.data(),
                              pimpl_->output_tensors_.data(),
                              pimpl_->output_tensors_.size());

        auto end = std::chrono::high_resolution_clock::now();
        result.inference_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.success = true;
    } catch (const Ort::Exception& e) {
        result.error = std::string("ONNX Runtime error: ") + e.what();
    } catch (const std::exception& e) {
        result.error = std::string("Error: ") + e.what();
    }

    return result;
}

InferenceResult OnnxInferenceEngine::runNamed(const std::vector<std::pair<std::string, const float*>>& inputs,
                                              std::vector<std::pair<std::string, float*>>& outputs) {
    InferenceResult result;
    result.success = false;

    if (!pimpl_->initialized_) {
        result.error = "Engine not initialized";
        return result;
    }

    if (inputs.size() != pimpl_->input_infos_.size() || outputs.size() != pimpl_->output_infos_.size()) {
        result.error = "Input/output count mismatch";
        return result;
    }

    auto start = std::chrono::high_resolution_clock::now();

    try {
        pimpl_->allocateNamedInputTensors(inputs);
        pimpl_->allocateNamedOutputTensors(outputs);

        if (!pimpl_->last_error_.empty()) {
            result.error = pimpl_->last_error_;
            return result;
        }

        pimpl_->session_->Run(*pimpl_->run_options_,
                              pimpl_->input_names_c_.data(),
                              pimpl_->input_tensors_.data(),
                              pimpl_->input_tensors_.size(),
                              pimpl_->output_names_c_.data(),
                              pimpl_->output_tensors_.data(),
                              pimpl_->output_tensors_.size());

        auto end = std::chrono::high_resolution_clock::now();
        result.inference_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.success = true;
    } catch (const Ort::Exception& e) {
        result.error = std::string("ONNX Runtime error: ") + e.what();
    } catch (const std::exception& e) {
        result.error = std::string("Error: ") + e.what();
    }

    return result;
}

bool OnnxInferenceEngine::warmup() {
    if (!pimpl_->initialized_) {
        pimpl_->setError("Engine not initialized");
        return false;
    }

    try {
        // Create dummy inputs for warmup
        std::vector<const float*> dummy_inputs;
        std::vector<float*> dummy_outputs;
        std::vector<std::vector<float>> input_buffers;
        std::vector<std::vector<float>> output_buffers;

        for (const auto& info : pimpl_->input_infos_) {
            size_t elements = info.element_count();
            std::vector<int64_t> shape = info.shape;
            for (auto& dim : shape) if (dim == -1) dim = 1;
            elements = 1;
            for (int64_t dim : shape) elements *= static_cast<size_t>(dim);
            
            input_buffers.emplace_back(elements, 0.0f);
            dummy_inputs.push_back(input_buffers.back().data());
        }

        for (const auto& info : pimpl_->output_infos_) {
            size_t elements = info.element_count();
            std::vector<int64_t> shape = info.shape;
            for (auto& dim : shape) if (dim == -1) dim = 1;
            elements = 1;
            for (int64_t dim : shape) elements *= static_cast<size_t>(dim);
            
            output_buffers.emplace_back(elements, 0.0f);
            dummy_outputs.push_back(output_buffers.back().data());
        }

        // Run warmup inference
        for (int i = 0; i < 3; ++i) {
            auto warmup_result = run(dummy_inputs, dummy_outputs);
            if (!warmup_result.success) {
                pimpl_->setError("Warmup failed: " + warmup_result.error);
                return false;
            }
        }

        // Reset streaming state after warmup
        reset();
        return true;
    } catch (const std::exception& e) {
        pimpl_->setError("Warmup error: " + std::string(e.what()));
        return false;
    }
}

void OnnxInferenceEngine::reset() {
    // Reset any streaming state buffers
    pimpl_->state_buffers_.clear();
}

bool OnnxInferenceEngine::isReady() const {
    return pimpl_->initialized_ && pimpl_->session_ != nullptr;
}

std::string OnnxInferenceEngine::getLastError() const {
    return pimpl_->last_error_;
}

} // namespace rtvcc