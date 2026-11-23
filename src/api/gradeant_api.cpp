#include "gradeant_api.h"

#include "../core/detection.hpp"
#include "../core/generation.hpp"

#include <opencv2/imgcodecs.hpp>

#include <exception>
#include <memory>
#include <string>

struct GradeAntResult {
    gradeant::Document document;
    std::string ir_json;
    std::string html;
};

namespace {
thread_local std::string error_buffer;
thread_local int32_t error_code = GRADEANT_STATUS_SUCCESS;

void clear_error() {
    error_buffer.clear();
    error_code = GRADEANT_STATUS_SUCCESS;
}

int32_t report_error(int32_t code, const char* message) {
    error_code = code;
    error_buffer = message == nullptr ? "Unknown native error." : message;
    return code;
}

int32_t validate_compile_options(const GradeAntCompileOptions* options) {
    if (options == nullptr) return GRADEANT_STATUS_SUCCESS;
    if (options->struct_size < sizeof(GradeAntCompileOptions) ||
        options->api_version != GRADEANT_API_VERSION) {
        return report_error(GRADEANT_STATUS_INVALID_OPTIONS,
                            "Compile options have an unsupported size or API version.");
    }
    return GRADEANT_STATUS_SUCCESS;
}

bool profile_from_options(const GradeAntGenerateOptions* options,
                          gradeant::ReconstructionProfile& profile) {
    if (options == nullptr) {
        profile = gradeant::ReconstructionProfile::hybrid;
        return true;
    }
    if (options->struct_size < sizeof(GradeAntGenerateOptions) ||
        options->api_version != GRADEANT_API_VERSION || options->flags != 0) {
        report_error(GRADEANT_STATUS_INVALID_OPTIONS,
                     "Generation options have an unsupported size, API version, or flags.");
        return false;
    }
    switch (options->profile) {
    case GRADEANT_PROFILE_FIDELITY: profile = gradeant::ReconstructionProfile::fidelity; return true;
    case GRADEANT_PROFILE_SEMANTIC: profile = gradeant::ReconstructionProfile::semantic; return true;
    case GRADEANT_PROFILE_HYBRID: profile = gradeant::ReconstructionProfile::hybrid; return true;
    default:
        report_error(GRADEANT_STATUS_INVALID_OPTIONS, "Unknown reconstruction profile.");
        return false;
    }
}

} // namespace

extern "C" GRADEANT_API uint32_t gradeant_api_version_v1(void) {
    return GRADEANT_API_VERSION;
}

extern "C" GRADEANT_API int32_t gradeant_compile_image_v1(
    const char* image_path, const GradeAntCompileOptions* options, GradeAntResult** result) {
    clear_error();
    if (result == nullptr) return report_error(GRADEANT_STATUS_INVALID_ARGUMENT, "Result output pointer is null.");
    *result = nullptr;
    if (image_path == nullptr || *image_path == '\0')
        return report_error(GRADEANT_STATUS_INVALID_ARGUMENT, "Image path is empty.");
    const int32_t options_status = validate_compile_options(options);
    if (options_status != GRADEANT_STATUS_SUCCESS) return options_status;

    try {
        if (options != nullptr && options->progress_callback != nullptr)
            options->progress_callback(0, "Loading image", options->user_data);
        const cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
        if (image.empty())
            return report_error(GRADEANT_STATUS_IMAGE_LOAD_FAILED,
                                "OpenCV could not read the image. Check the path and image format.");
        if (options != nullptr && options->progress_callback != nullptr)
            options->progress_callback(25, "Detecting regions", options->user_data);
        auto compiled = std::make_unique<GradeAntResult>();
        compiled->document = gradeant::detect_interface(image);
        if (options != nullptr && options->progress_callback != nullptr)
            options->progress_callback(85, "Validating interface", options->user_data);
        compiled->ir_json = gradeant::to_json(compiled->document);
        if (options != nullptr && options->progress_callback != nullptr)
            options->progress_callback(100, "Compilation complete", options->user_data);
        *result = compiled.release();
        return GRADEANT_STATUS_SUCCESS;
    } catch (const std::exception& error) {
        return report_error(GRADEANT_STATUS_COMPILATION_FAILED, error.what());
    } catch (...) {
        return report_error(GRADEANT_STATUS_COMPILATION_FAILED,
                            "Unknown error while compiling the layout.");
    }
}

extern "C" GRADEANT_API const char* gradeant_result_get_ir_json(const GradeAntResult* result) {
    if (result == nullptr) {
        report_error(GRADEANT_STATUS_INVALID_ARGUMENT, "Compilation result is null.");
        return nullptr;
    }
    return result->ir_json.c_str();
}

extern "C" GRADEANT_API const char* gradeant_result_generate_html_v1(
    GradeAntResult* result, const GradeAntGenerateOptions* options) {
    clear_error();
    if (result == nullptr) {
        report_error(GRADEANT_STATUS_INVALID_ARGUMENT, "Compilation result is null.");
        return nullptr;
    }
    gradeant::ReconstructionProfile profile;
    if (!profile_from_options(options, profile)) return nullptr;
    try {
        result->html = gradeant::generate_html(result->document, profile);
        return result->html.c_str();
    } catch (const std::exception& error) {
        report_error(GRADEANT_STATUS_COMPILATION_FAILED, error.what());
    } catch (...) {
        report_error(GRADEANT_STATUS_COMPILATION_FAILED, "Unknown error while generating HTML.");
    }
    return nullptr;
}

extern "C" GRADEANT_API void gradeant_result_release(GradeAntResult* result) {
    delete result;
}

extern "C" GRADEANT_API int32_t gradeant_get_last_error_code_v1(void) {
    return error_code;
}

extern "C" GRADEANT_API const char* gradeant_get_last_error_v1(void) {
    return error_buffer.c_str();
}
