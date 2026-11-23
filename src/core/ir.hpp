#pragma once

#include <map>
#include <string>
#include <vector>

namespace gradeant {

inline constexpr int ir_schema_version = 1;

struct Bounds {
    int x{};
    int y{};
    int width{};
    int height{};
};

enum class LayoutKind {
    absolute,
    horizontal,
    vertical,
    grid
};

struct VisualStyle {
    std::string foreground;
    std::string background;
    std::string border_color;
    int border_width{};
    int border_radius{};
};

struct Layout {
    LayoutKind kind{LayoutKind::absolute};
    int gap{};
    int columns{};
};

struct Component {
    std::string id;
    std::string type{"container"};
    Bounds bounds;
    std::string text;
    VisualStyle style;
    Layout layout;
    std::vector<std::string> constraints;
    std::string asset;
    double confidence{};
    std::map<std::string, std::string> properties;
    std::vector<Component> children;
};

struct Document {
    int schema_version{ir_schema_version};
    int width{};
    int height{};
    std::map<std::string, std::string> metadata;
    Layout layout;
    std::vector<Component> components;
};

enum class ReconstructionProfile {
    fidelity = 0,
    semantic = 1,
    hybrid = 2
};

std::vector<std::string> validate(const Document& document);
std::string to_json(const Document& document);
std::string layout_kind_name(LayoutKind kind);

} // namespace gradeant
