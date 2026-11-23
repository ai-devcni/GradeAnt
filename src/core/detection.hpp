#pragma once

#include "ir.hpp"

#include <opencv2/core/mat.hpp>

namespace gradeant {

Document detect_interface(const cv::Mat& image);

} // namespace gradeant
