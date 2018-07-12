/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <iomanip>
#include <vector>
#include <memory>
#include <string>
#include <cstdlib>

#include <opencv2/opencv.hpp>
#include <inference_engine.hpp>

using namespace InferenceEngine;

int main(int argc, char *argv[]) {
    try {
        if (argc != 3) {
            std::cout << "Usage : ./hello_classification <path_to_model> <path_to_image>" << std::endl;
            return EXIT_FAILURE;
        }
        const std::string input_model{argv[1]};
        const std::string input_image_path{argv[2]};

        // ---------------------Load MKLDNN Plugin for Inference Engine-----------------------------------------

        InferenceEngine::PluginDispatcher dispatcher({"../../../lib/intel64", ""});
        InferencePlugin plugin(dispatcher.getSuitablePlugin(TargetDevice::eMYRIAD));

        // --------------------Load IR Generated by ModelOptimizer (.xml and .bin files)------------------------

        CNNNetReader network_reader;
        network_reader.ReadNetwork(input_model);
        network_reader.ReadWeights(input_model.substr(0, input_model.size() - 4) + ".bin");
        network_reader.getNetwork().setBatchSize(1);
        CNNNetwork network = network_reader.getNetwork();

        // -----------------------------Prepare input blobs-----------------------------------------------------

        auto input_info = network.getInputsInfo().begin()->second;
        auto input_name = network.getInputsInfo().begin()->first;

        input_info->setPrecision(Precision::U8);

        // ---------------------------Prepare output blobs------------------------------------------------------

        auto output_info = network.getOutputsInfo().begin()->second;
        auto output_name = network.getOutputsInfo().begin()->first;

        output_info->setPrecision(Precision::FP32);

        // -------------------------Loading model to the plugin and then infer----------------------------------

        auto executable_network = plugin.LoadNetwork(network, {});
        auto infer_request = executable_network.CreateInferRequest();

        auto input = infer_request.GetBlob(input_name);
        auto input_data = input->buffer().as<PrecisionTrait<Precision::U8>::value_type*>();

        /* Copying data from image to the input blob */
        cv::Mat image = cv::imread(input_image_path);
        cv::resize(image, image, cv::Size(input_info->getDims()[0], input_info->getDims()[1]));

        size_t channels_number = input->dims()[2];
        size_t image_size = input->dims()[1] * input->dims()[0];

        for (size_t pid = 0; pid < image_size; ++pid) {
            for (size_t ch = 0; ch < channels_number; ++ch) {
                input_data[ch * image_size + pid] = image.at<cv::Vec3b>(pid)[ch];
            }
        }

        /* Running the request synchronously */
        infer_request.Infer();

        // ---------------------------Postprocess output blobs--------------------------------------------------

        auto output = infer_request.GetBlob(output_name);
        auto output_data = output->buffer().as<PrecisionTrait<Precision::FP32>::value_type*>();

        std::vector<unsigned> results;
        /*  This is to sort output probabilities and put it to results vector */
        TopResults(10, *output, results);

        std::cout << std::endl << "Top 10 results:" << std::endl << std::endl;
        for (size_t id = 0; id < 10; ++id) {
            std::cout.precision(7);
            auto result = output_data[results[id]];
            std::cout << std::left << std::fixed << result << " label #" << results[id] << std::endl;
        }
    } catch (const std::exception & ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
