#include "detection.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gradeant {
namespace {

struct DetectedRegion {
    Bounds bounds;
    std::string type;
    int parent{-1};
};

std::int64_t area(const Bounds& bounds) {
    return static_cast<std::int64_t>(bounds.width) * bounds.height;
}

bool contains(const Bounds& outer, const Bounds& inner) {
    constexpr int padding = 2;
    return inner.x >= outer.x + padding && inner.y >= outer.y + padding &&
           inner.x + inner.width <= outer.x + outer.width - padding &&
           inner.y + inner.height <= outer.y + outer.height - padding;
}

std::vector<DetectedRegion> detect_regions(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 1) gray = image;
    else if (image.channels() == 3) cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    else if (image.channels() == 4) cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    else throw std::runtime_error("The image must have 1, 3, or 4 channels.");

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    cv::Mat thresholded;
    cv::threshold(blurred, thresholded, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresholded, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    const std::int64_t image_area = static_cast<std::int64_t>(image.cols) * image.rows;
    std::vector<DetectedRegion> regions;
    for (const auto& contour : contours) {
        const cv::Rect box = cv::boundingRect(contour);
        const Bounds bounds{box.x, box.y, box.width, box.height};
        const auto box_area = area(bounds);
        // Discard tiny contours and near-full-image regions before classifying their shape.
        if (box.width < 18 || box.height < 12 || box_area < 220 || box_area > image_area * 0.92) continue;
        const double ratio = static_cast<double>(box.width) / box.height;
        std::string type = "container";
        if (ratio >= 2.7 && box.height <= image.rows * 0.16) type = "input";
        else if (ratio >= 1.15 && ratio <= 4.2 && box.height <= image.rows * 0.2) type = "button";
        regions.push_back({bounds, std::move(type), -1});
    }

    std::sort(regions.begin(), regions.end(), [](const DetectedRegion& first, const DetectedRegion& second) {
        return area(first.bounds) > area(second.bounds);
    });
    std::vector<DetectedRegion> unique;
    for (const auto& region : regions) {
        // RETR_LIST may return several nearby contours for one visual region.
        const bool duplicate = std::any_of(unique.begin(), unique.end(), [&](const DetectedRegion& kept) {
            return std::abs(kept.bounds.x - region.bounds.x) <= 3 &&
                   std::abs(kept.bounds.y - region.bounds.y) <= 3 &&
                   std::abs(kept.bounds.width - region.bounds.width) <= 6 &&
                   std::abs(kept.bounds.height - region.bounds.height) <= 6;
        });
        if (!duplicate) unique.push_back(region);
        if (unique.size() >= 250) break;
    }

    // Choose the smallest enclosing region so nested HTML follows the nearest detected parent.
    for (std::size_t child = 0; child < unique.size(); ++child) {
        std::int64_t smallest_area = std::numeric_limits<std::int64_t>::max();
        for (std::size_t candidate = 0; candidate < unique.size(); ++candidate) {
            if (candidate == child || !contains(unique[candidate].bounds, unique[child].bounds)) continue;
            const auto candidate_area = area(unique[candidate].bounds);
            if (candidate_area > area(unique[child].bounds) && candidate_area < smallest_area) {
                unique[child].parent = static_cast<int>(candidate);
                smallest_area = candidate_area;
            }
        }
    }
    return unique;
}

Layout infer_layout(const Bounds& parent, const std::vector<const DetectedRegion*>& children) {
    Layout result;
    if (children.size() < 2) return result;

    std::vector<const DetectedRegion*> ordered = children;
    std::sort(ordered.begin(), ordered.end(), [](const auto* first, const auto* second) {
        if (first->bounds.y != second->bounds.y) return first->bounds.y < second->bounds.y;
        return first->bounds.x < second->bounds.x;
    });
    bool horizontal = true;
    bool vertical = true;
    for (std::size_t index = 1; index < ordered.size(); ++index) {
        const Bounds& previous = ordered[index - 1]->bounds;
        const Bounds& current = ordered[index]->bounds;
        const double vertical_delta = std::abs((previous.y + previous.height / 2.0) -
                                               (current.y + current.height / 2.0));
        const double horizontal_delta = std::abs((previous.x + previous.width / 2.0) -
                                                 (current.x + current.width / 2.0));
        horizontal = horizontal && vertical_delta <= std::max(4.0, std::min(previous.height, current.height) * 0.2) &&
                     current.x >= previous.x + previous.width;
        vertical = vertical && horizontal_delta <= std::max(4.0, std::min(previous.width, current.width) * 0.2) &&
                   current.y >= previous.y + previous.height;
    }
    // Infer flow only when every adjacent pair supports the same alignment and ordering.
    if (horizontal) result.kind = LayoutKind::horizontal;
    else if (vertical) result.kind = LayoutKind::vertical;
    else return result;

    int total_gap = 0;
    for (std::size_t index = 1; index < ordered.size(); ++index) {
        const Bounds& previous = ordered[index - 1]->bounds;
        const Bounds& current = ordered[index]->bounds;
        total_gap += result.kind == LayoutKind::horizontal
                         ? current.x - (previous.x + previous.width)
                         : current.y - (previous.y + previous.height);
    }
    result.gap = std::max(0, total_gap / static_cast<int>(ordered.size() - 1));
    if (parent.width <= 0 || parent.height <= 0) return Layout{};
    return result;
}

Component make_component(const DetectedRegion& region, const std::vector<DetectedRegion>& regions,
                         int index) {
    Component component;
    component.id = "component-" + std::to_string(index + 1);
    component.type = region.type;
    component.bounds = region.bounds;
    // This is a conservative heuristic score, not a measured accuracy value.
    component.confidence = 0.35;
    component.style.background = "#ffffff";
    component.style.border_color = "#cbd5e1";
    component.style.border_width = 1;
    component.style.border_radius = 6;
    component.properties["classification"] = "geometry-heuristic";
    std::vector<const DetectedRegion*> child_regions;
    for (const auto& candidate : regions)
        if (candidate.parent == index) child_regions.push_back(&candidate);
    component.layout = infer_layout(region.bounds, child_regions);
    for (std::size_t child = 0; child < regions.size(); ++child)
        if (regions[child].parent == index)
            component.children.push_back(make_component(regions[child], regions, static_cast<int>(child)));
    std::sort(component.children.begin(), component.children.end(), [](const Component& first, const Component& second) {
        if (first.bounds.y != second.bounds.y) return first.bounds.y < second.bounds.y;
        return first.bounds.x < second.bounds.x;
    });
    return component;
}

} // namespace

Document detect_interface(const cv::Mat& image) {
    if (image.empty()) throw std::runtime_error("OpenCV could not read the image. Check the path and image format.");
    if (image.cols < 1 || image.rows < 1) throw std::runtime_error("Image has invalid dimensions.");
    Document document;
    document.width = image.cols;
    document.height = image.rows;
    document.metadata["source"] = "opencv-contour-detector";
    document.layout.kind = LayoutKind::absolute;
    auto regions = detect_regions(image);
    std::vector<const DetectedRegion*> root_regions;
    for (const auto& region : regions)
        if (region.parent < 0) root_regions.push_back(&region);
    document.layout = infer_layout(Bounds{0, 0, image.cols, image.rows}, root_regions);
    for (std::size_t index = 0; index < regions.size(); ++index)
        if (regions[index].parent < 0)
            document.components.push_back(make_component(regions[index], regions, static_cast<int>(index)));
    std::sort(document.components.begin(), document.components.end(), [](const Component& first, const Component& second) {
        if (first.bounds.y != second.bounds.y) return first.bounds.y < second.bounds.y;
        return first.bounds.x < second.bounds.x;
    });
    const auto errors = validate(document);
    if (!errors.empty()) throw std::runtime_error(errors.front());
    return document;
}

} // namespace gradeant
