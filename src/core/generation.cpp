#include "generation.hpp"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace gradeant {
namespace {

std::string html_escape(const std::string& value) {
    std::string escaped;
    for (const char character : value) {
        switch (character) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped += character; break;
        }
    }
    return escaped;
}

bool safe_hex_colour(const std::string& value) {
    if (value.empty() || value[0] != '#') return false;
    const std::size_t digits = value.size() - 1;
    if (digits != 3 && digits != 4 && digits != 6 && digits != 8) return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f') ||
               (character >= 'A' && character <= 'F');
    });
}

const char* safe_colour(const std::string& value, const char* fallback) {
    return safe_hex_colour(value) ? value.c_str() : fallback;
}

const char* safe_type(const std::string& type) {
    if (type == "button" || type == "input") return type.c_str();
    return "container";
}

void write_layout_css(std::ostream& css, const char* selector, const Layout& layout,
                      ReconstructionProfile profile);

Layout effective_layout(Layout layout, std::size_t child_count, ReconstructionProfile profile) {
    if (profile == ReconstructionProfile::fidelity) return Layout{};
    if (profile == ReconstructionProfile::semantic &&
        layout.kind == LayoutKind::absolute && child_count > 1) {
        layout.kind = LayoutKind::vertical;
    }
    return layout;
}

double percent(int value, int total) {
    return total > 0 ? 100.0 * value / total : 0.0;
}

void write_size(std::ostream& css, const Bounds& bounds, const Bounds& parent,
                LayoutKind parent_layout, int siblings, int gap) {
    const double width = percent(bounds.width, parent.width);
    const double height = percent(bounds.height, parent.height);
    css << "  width: " << std::fixed << std::setprecision(4) << width << "%;\n"
        << "  height: " << height << "%;\n";
    if (parent_layout == LayoutKind::horizontal && siblings > 1) {
        const double share = static_cast<double>(gap) * (siblings - 1) / siblings;
        css << "  flex: 0 0 calc(" << width << "% - " << std::setprecision(2) << share << "px);\n";
    } else if (parent_layout == LayoutKind::vertical && siblings > 1) {
        const double share = static_cast<double>(gap) * (siblings - 1) / siblings;
        css << "  flex: 0 0 calc(" << height << "% - " << std::setprecision(2) << share << "px);\n";
    } else if (parent_layout == LayoutKind::grid) {
        css << "  grid-column: span 1;\n";
    } else {
        css << "  flex: 0 0 auto;\n";
    }
}

void write_component_css(std::ostream& css, const Component& component, const Bounds& parent,
                         LayoutKind parent_layout, int siblings, int parent_gap, int index,
                         ReconstructionProfile profile, const Layout& component_layout) {
    const bool flow = profile != ReconstructionProfile::fidelity &&
                      parent_layout != LayoutKind::absolute;
    css << ".component--" << index << " {\n";
    css << "  " << (flow ? "position: relative;\n" : "position: absolute;\n");
    if (flow) {
        write_size(css, component.bounds, parent, parent_layout, siblings,
                   parent_gap);
    } else {
        const int x = component.bounds.x - parent.x;
        const int y = component.bounds.y - parent.y;
        css << "  left: " << std::fixed << std::setprecision(4) << percent(x, parent.width) << "%;\n"
            << "  top: " << percent(y, parent.height) << "%;\n"
            << "  width: " << percent(component.bounds.width, parent.width) << "%;\n"
            << "  height: " << percent(component.bounds.height, parent.height) << "%;\n";
    }
    css << "  box-sizing: border-box;\n"
        << "  border: " << component.style.border_width << "px solid "
        << safe_colour(component.style.border_color, "#cbd5e1") << ";\n"
        << "  border-radius: " << component.style.border_radius << "px;\n"
        << "  color: " << safe_colour(component.style.foreground, "#1f2937") << ";\n"
        << "  background: " << safe_colour(component.style.background, "#ffffff") << ";\n"
        << "}\n";
    write_layout_css(css, (".component--" + std::to_string(index)).c_str(),
                     component_layout, profile);
}

void write_layout_css(std::ostream& css, const char* selector, const Layout& layout,
                      ReconstructionProfile profile) {
    if (profile == ReconstructionProfile::fidelity || layout.kind == LayoutKind::absolute) return;
    css << selector << " {\n";
    if (layout.kind == LayoutKind::grid) {
        css << "  display: grid;\n  grid-template-columns: repeat("
            << std::max(1, layout.columns) << ", minmax(0, 1fr));\n";
    } else {
        css << "  display: flex;\n  flex-direction: "
            << (layout.kind == LayoutKind::horizontal ? "row" : "column") << ";\n";
    }
    css << "  gap: " << layout.gap << "px;\n  align-items: flex-start;\n}\n";
}

void write_component_html(std::ostream& html, const Component& component,
                          int& class_index, int depth) {
    const int current = class_index++;
    const char* type = safe_type(component.type);
    const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
    const char* tag = component.type == "button" ? "button" :
                      component.type == "input" ? "input" : "section";
    html << indent << '<' << tag << " class=\"component component--" << current
         << " component--" << type << "\" data-component-id=\""
         << html_escape(component.id) << "\"";
    if (component.type == "button")
        html << " type=\"button\" aria-label=\"Detected button\"";
    else if (component.type == "input")
        html << " type=\"text\" aria-label=\"Detected text field\"";
    else
        html << " aria-label=\"Detected layout container\"";
    if (component.type == "input") {
        html << ">\n";
        return;
    }
    html << ">\n";
    if (!component.text.empty())
        html << indent << "  " << html_escape(component.text) << "\n";
    for (const auto& child : component.children)
        write_component_html(html, child, class_index, depth + 1);
    html << indent << "</" << tag << ">\n";
}

void write_component_css_tree(std::ostream& css, const Component& component, const Bounds& parent,
                              LayoutKind parent_layout, int siblings, int parent_gap,
                              int& class_index, ReconstructionProfile profile) {
    const int current = class_index++;
    const auto component_layout = effective_layout(component.layout, component.children.size(), profile);
    write_component_css(css, component, parent, parent_layout, siblings, parent_gap, current, profile,
                        component_layout);
    for (const auto& child : component.children)
        write_component_css_tree(css, child, component.bounds, component_layout.kind,
                                 static_cast<int>(component.children.size()), component_layout.gap,
                                 class_index, profile);
}

} // namespace

std::string generate_html(const Document& document, ReconstructionProfile profile) {
    const auto errors = validate(document);
    if (!errors.empty()) throw std::invalid_argument(errors.front());

    std::ostringstream html;
    std::ostringstream css;
    html.imbue(std::locale::classic());
    css.imbue(std::locale::classic());
    const Bounds document_bounds{0, 0, document.width, document.height};
    const auto root_layout = effective_layout(document.layout, document.components.size(), profile);
    css << "* { box-sizing: border-box; }\n"
        << "body { margin: 0; min-height: 100vh; display: grid; place-items: center; "
           "background: #f4f6f8; font-family: system-ui, sans-serif; }\n"
        << ".layout { position: relative; width: min(100%, " << document.width
        << "px); aspect-ratio: " << document.width << " / " << document.height
        << "; background: #fff; overflow: hidden; }\n";
    write_layout_css(css, ".layout", root_layout, profile);
    int class_index = 0;
    for (const auto& component : document.components)
        write_component_css_tree(css, component, document_bounds, root_layout.kind,
                                 static_cast<int>(document.components.size()), root_layout.gap,
                                 class_index, profile);

    html << "<!doctype html>\n<html lang=\"en\">\n<head>\n"
         << "  <meta charset=\"utf-8\">\n"
         << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
         << "  <title>Generated layout</title>\n  <style>\n" << css.str()
         << "</style>\n</head>\n<body>\n"
         << "  <main class=\"layout\" aria-label=\"Generated interface layout\">\n";
    class_index = 0;
    for (const auto& component : document.components)
        write_component_html(html, component, class_index, 2);
    html << "  </main>\n</body>\n</html>\n";
    return html.str();
}

} // namespace gradeant
