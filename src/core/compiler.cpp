#include "compiler.hpp"

#include <memory>
#include <utility>

namespace {
using ResultPointer = std::unique_ptr<GradeAntResult, decltype(&gradeant_result_release)>;
// Preserve the legacy return-pointer lifetime until the next call on this thread.
thread_local ResultPointer legacy_result(nullptr, &gradeant_result_release);
}

extern "C" GRADEANT_API const char* compile_image_layout(const char* image_path) {
    legacy_result.reset();
    GradeAntResult* raw_result = nullptr;
    if (gradeant_compile_image_v1(image_path, nullptr, &raw_result) != GRADEANT_STATUS_SUCCESS)
        return nullptr;
    ResultPointer result(raw_result, &gradeant_result_release);

    GradeAntGenerateOptions options{
        sizeof(GradeAntGenerateOptions), GRADEANT_API_VERSION, GRADEANT_PROFILE_HYBRID, 0
    };
    const char* html = gradeant_result_generate_html_v1(result.get(), &options);
    if (html == nullptr) return nullptr;
    legacy_result = std::move(result);
    return html;
}

extern "C" GRADEANT_API const char* get_last_error(void) {
    return gradeant_get_last_error_v1();
}
