#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  if defined(GRADEANT_BUILDING_LIBRARY)
#    define GRADEANT_API __declspec(dllexport)
#  else
#    define GRADEANT_API __declspec(dllimport)
#  endif
#else
#  define GRADEANT_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GradeAntResult GradeAntResult;
typedef void (*GradeAntProgressCallback)(int32_t percent, const char* stage, void* user_data);

typedef struct GradeAntCompileOptions {
    uint32_t struct_size;
    uint32_t api_version;
    GradeAntProgressCallback progress_callback;
    void* user_data;
} GradeAntCompileOptions;

typedef struct GradeAntGenerateOptions {
    uint32_t struct_size;
    uint32_t api_version;
    int32_t profile;
    uint32_t flags;
} GradeAntGenerateOptions;

enum {
    GRADEANT_API_VERSION = 1,
    GRADEANT_PROFILE_FIDELITY = 0,
    GRADEANT_PROFILE_SEMANTIC = 1,
    GRADEANT_PROFILE_HYBRID = 2,
    GRADEANT_STATUS_SUCCESS = 0,
    GRADEANT_STATUS_INVALID_ARGUMENT = 1,
    GRADEANT_STATUS_IMAGE_LOAD_FAILED = 2,
    GRADEANT_STATUS_INVALID_OPTIONS = 3,
    GRADEANT_STATUS_COMPILATION_FAILED = 4
};

GRADEANT_API uint32_t gradeant_api_version_v1(void);
/* A successful compile returns an owned handle; release it exactly once.
   The progress callback runs synchronously on the calling thread and must not throw. */
GRADEANT_API int32_t gradeant_compile_image_v1(
    const char* image_path,
    const GradeAntCompileOptions* options,
    GradeAntResult** result);
/* IR text is owned by result and remains valid until result is released. */
GRADEANT_API const char* gradeant_result_get_ir_json(const GradeAntResult* result);
/* Generated HTML remains valid until the next generation call on result or release. */
GRADEANT_API const char* gradeant_result_generate_html_v1(
    GradeAntResult* result,
    const GradeAntGenerateOptions* options);
GRADEANT_API void gradeant_result_release(GradeAntResult* result);
GRADEANT_API int32_t gradeant_get_last_error_code_v1(void);
GRADEANT_API const char* gradeant_get_last_error_v1(void);

/* Legacy HTML is valid until the next compile_image_layout call on this thread. */
GRADEANT_API const char* compile_image_layout(const char* image_path);
/* The returned thread-local error text is valid until the next API operation that updates it. */
GRADEANT_API const char* get_last_error(void);

#ifdef __cplusplus
}
#endif
