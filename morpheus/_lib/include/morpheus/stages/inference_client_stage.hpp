/*
 * SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "morpheus/export.h"
#include "morpheus/messages/control.hpp"
#include "morpheus/messages/multi_inference.hpp"
#include "morpheus/messages/multi_response.hpp"
#include "morpheus/types.hpp"

#include <boost/fiber/policy.hpp>
#include <mrc/coroutines/async_generator.hpp>
#include <mrc/coroutines/scheduler.hpp>
#include <mrc/coroutines/task.hpp>
#include <mrc/segment/builder.hpp>
#include <mrc/segment/object.hpp>
#include <pybind11/pybind11.h>
#include <pymrc/asyncio_runnable.hpp>
#include <rxcpp/rx.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace morpheus {

struct MORPHEUS_EXPORT TensorModelMapping
{
    /**
     * @brief The field name to/from the model used for mapping
     */
    std::string model_field_name;

    /**
     * @brief The field name to/from the tensor used for mapping
     */
    std::string tensor_field_name;
};

class MORPHEUS_EXPORT IInferenceClientSession
{
  public:
    virtual ~IInferenceClientSession() = default;
    /**
      @brief Gets the inference input mappings
    */
    virtual std::vector<TensorModelMapping> get_input_mappings(std::vector<TensorModelMapping> input_map_overrides) = 0;

    /**
      @brief Gets the inference output mappings
    */
    virtual std::vector<TensorModelMapping> get_output_mappings(
        std::vector<TensorModelMapping> output_map_overrides) = 0;

    /**
      @brief Invokes a single tensor inference
    */
    virtual mrc::coroutines::Task<TensorMap> infer(TensorMap&& inputs) = 0;
};

class MORPHEUS_EXPORT IInferenceClient
{
  public:
    virtual ~IInferenceClient() = default;
    /**
      @brief Creates an inference session.
    */
    virtual std::unique_ptr<IInferenceClientSession> create_session() = 0;
};

/**
 * @addtogroup stages
 * @{
 * @file
 */

/**
 * @brief Perform inference with Triton Inference Server.
 * This class specifies which inference implementation category (Ex: NLP/FIL) is needed for inferencing.
 */
template <typename InputT, typename OutputT>
class MORPHEUS_EXPORT InferenceClientStage
  : public mrc::pymrc::AsyncioRunnable<std::shared_ptr<InputT>, std::shared_ptr<OutputT>>
{
  public:
    using sink_type_t   = std::shared_ptr<InputT>;
    using source_type_t = std::shared_ptr<OutputT>;

    /**
     * @brief Construct a new Inference Client Stage object
     *
     * @param client : Inference client instance.
     * @param model_name : Name of the model specifies which model can handle the inference requests that are sent to
     * Triton inference
     * @param needs_logits : Determines if logits are required.
     * @param inout_mapping : Dictionary used to map pipeline input/output names to Triton input/output names. Use this
     * if the Morpheus names do not match the model.
     */
    InferenceClientStage(std::unique_ptr<IInferenceClient>&& client,
                         std::string model_name,
                         bool needs_logits,
                         std::vector<TensorModelMapping> input_mapping,
                         std::vector<TensorModelMapping> output_mapping);

    /**
     * Process a single InputT by running the constructor-provided inference client against it's Tensor,
     * and yields the result as a OutputT
     */
    mrc::coroutines::AsyncGenerator<std::shared_ptr<OutputT>> on_data(
        std::shared_ptr<InputT>&& data, std::shared_ptr<mrc::coroutines::Scheduler> on) override;

  private:
    std::string m_model_name;
    std::shared_ptr<IInferenceClient> m_client;
    std::shared_ptr<IInferenceClientSession> m_session;
    bool m_needs_logits{true};
    std::vector<TensorModelMapping> m_input_mapping;
    std::vector<TensorModelMapping> m_output_mapping;
    std::mutex m_session_mutex;

    int32_t m_retry_max = 10;
};

/****** InferenceClientStageInferenceProxy******************/
/**
 * @brief Interface proxy, used to insulate python bindings.
 */
struct MORPHEUS_EXPORT InferenceClientStageInterfaceProxy
{
    /**
     * @brief Create and initialize a MultiMessage-based InferenceClientStage, and return the result
     *
     * @param builder : Pipeline context object reference
     * @param name : Name of a stage reference
     * @param model_name : Name of the model specifies which model can handle the inference requests that are sent to
     * Triton inference
     * @param server_url : Triton server URL.
     * @param needs_logits : Determines if logits are required.
     * @param inout_mapping : Dictionary used to map pipeline input/output names to Triton input/output names. Use this
     * if the Morpheus names do not match the model.
     * @return std::shared_ptr<mrc::segment::Object<InferenceClientStage<MultiInferenceMessage, MultiResponseMessage>>>
     */
    static std::shared_ptr<mrc::segment::Object<InferenceClientStage<MultiInferenceMessage, MultiResponseMessage>>>
    init_mm(mrc::segment::Builder& builder,
            const std::string& name,
            std::string model_name,
            std::string server_url,
            bool needs_logits,
            std::map<std::string, std::string> input_mapping,
            std::map<std::string, std::string> output_mapping);

    /**
     * @brief Create and initialize a ControlMessage-based InferenceClientStage, and return the result
     *
     * @param builder : Pipeline context object reference
     * @param name : Name of a stage reference
     * @param model_name : Name of the model specifies which model can handle the inference requests that are sent to
     * Triton inference
     * @param server_url : Triton server URL.
     * @param needs_logits : Determines if logits are required.
     * @param inout_mapping : Dictionary used to map pipeline input/output names to Triton input/output names. Use this
     * if the Morpheus names do not match the model.
     * @return std::shared_ptr<mrc::segment::Object<InferenceClientStage<ControlMessage, ControlMessage>>>
     */
    static std::shared_ptr<mrc::segment::Object<InferenceClientStage<ControlMessage, ControlMessage>>> init_cm(
        mrc::segment::Builder& builder,
        const std::string& name,
        std::string model_name,
        std::string server_url,
        bool needs_logits,
        std::map<std::string, std::string> input_mapping,
        std::map<std::string, std::string> output_mapping);
};
/** @} */  // end of group

}  // namespace morpheus
