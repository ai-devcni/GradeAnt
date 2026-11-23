#include "../src/api/gradeant_api.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void report_progress(int32_t percent, const char*, void* user_data) {
    *static_cast<int32_t*>(user_data) = percent;
}

} // namespace

int main() {
    const auto unique_suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto image_path = std::filesystem::temp_directory_path() /
                            ("gradeant-api-test-" + std::to_string(unique_suffix) + ".png");
    try {
        cv::Mat image(160, 320, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::rectangle(image, cv::Point(20, 30), cv::Point(120, 80), cv::Scalar(0, 0, 0), cv::FILLED);
        cv::rectangle(image, cv::Point(180, 30), cv::Point(280, 80), cv::Scalar(0, 0, 0), cv::FILLED);
        require(cv::imwrite(image_path.string(), image), "Could not write the deterministic API test image.");

        int32_t progress = 0;
        GradeAntCompileOptions compile_options{
            sizeof(GradeAntCompileOptions), GRADEANT_API_VERSION, report_progress, &progress
        };
        GradeAntResult* result = nullptr;
        require(gradeant_compile_image_v1(image_path.string().c_str(), &compile_options, &result) ==
                    GRADEANT_STATUS_SUCCESS,
                "The versioned API should compile a readable image.");
        require(result != nullptr, "Successful compilation should return a result handle.");
        require(progress == 100, "The progress callback should report completion.");

        const char* ir = gradeant_result_get_ir_json(result);
        require(ir != nullptr && std::string(ir).find("\"schema_version\":1") != std::string::npos,
                "The result should expose versioned IR JSON.");

        GradeAntGenerateOptions fidelity{
            sizeof(GradeAntGenerateOptions), GRADEANT_API_VERSION, GRADEANT_PROFILE_FIDELITY, 0
        };
        const char* fidelity_html = gradeant_result_generate_html_v1(result, &fidelity);
        require(fidelity_html != nullptr && std::string(fidelity_html).find("position: absolute;") != std::string::npos,
                "The fidelity profile should generate positioned HTML.");

        GradeAntGenerateOptions hybrid{
            sizeof(GradeAntGenerateOptions), GRADEANT_API_VERSION, GRADEANT_PROFILE_HYBRID, 0
        };
        const char* hybrid_html = gradeant_result_generate_html_v1(result, &hybrid);
        require(hybrid_html != nullptr && std::string(hybrid_html).find("display: flex;") != std::string::npos,
                "The hybrid profile should use detected horizontal relationships.");

        GradeAntGenerateOptions invalid{
            sizeof(GradeAntGenerateOptions), GRADEANT_API_VERSION, GRADEANT_PROFILE_HYBRID, 1
        };
        require(gradeant_result_generate_html_v1(result, &invalid) == nullptr,
                "Unsupported generation flags must fail explicitly.");
        require(gradeant_get_last_error_code_v1() == GRADEANT_STATUS_INVALID_OPTIONS,
                "Invalid generation options should report an explicit status.");
        gradeant_result_release(result);

        result = nullptr;
        require(gradeant_compile_image_v1("", nullptr, &result) == GRADEANT_STATUS_INVALID_ARGUMENT,
                "An empty image path should be rejected.");
        require(result == nullptr && gradeant_get_last_error_v1()[0] != '\0',
                "Invalid input should leave a useful error and no result handle.");

        const char* compatible_html = compile_image_layout(image_path.string().c_str());
        require(compatible_html != nullptr && std::string(compatible_html).find("<!doctype html>") != std::string::npos,
                "The legacy compiler entry point should remain functional.");
        gradeant_result_release(nullptr);
        std::filesystem::remove(image_path);
        std::cout << "Native API tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove(image_path, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
