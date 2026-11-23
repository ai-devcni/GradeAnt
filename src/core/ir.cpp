#include "ir.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>

namespace gradeant {
namespace {

std::string escape_json(const std::string& value) {
    // JSON strings require escaped quotes, backslashes, and control characters.
    std::ostringstream escaped;
    for (const unsigned char character : value) {
        switch (character) {
        case '"': escaped << "\\\""; break;
        case '\\': escaped << "\\\\"; break;
        case '\b': escaped << "\\b"; break;
        case '\f': escaped << "\\f"; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default:
            if (character < 0x20) {
                escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(character) << std::dec;
            } else {
                escaped << static_cast<char>(character);
            }
        }
    }
    return escaped.str();
}

void write_string(std::ostream& output, const std::string& value) {
    output << '"' << escape_json(value) << '"';
}

void write_layout(std::ostream& output, const Layout& layout) {
    output << "{\"kind\":";
    write_string(output, layout_kind_name(layout.kind));
    output << ",\"gap\":" << layout.gap << ",\"columns\":" << layout.columns << '}';
}

void write_component(std::ostream& output, const Component& component) {
    // Keep this recursive representation aligned with the documented v1 JSON Schema.
    output << "{\"id\":";
    write_string(output, component.id);
    output << ",\"type\":";
    write_string(output, component.type);
    output << ",\"bounds\":{\"x\":" << component.bounds.x << ",\"y\":" << component.bounds.y
           << ",\"width\":" << component.bounds.width << ",\"height\":" << component.bounds.height
           << "},\"text\":";
    write_string(output, component.text);
    output << ",\"style\":{\"foreground\":";
    write_string(output, component.style.foreground);
    output << ",\"background\":";
    write_string(output, component.style.background);
    output << ",\"border_color\":";
    write_string(output, component.style.border_color);
    output << ",\"border_width\":" << component.style.border_width
           << ",\"border_radius\":" << component.style.border_radius << "},\"layout\":";
    write_layout(output, component.layout);
    output << ",\"constraints\":[";
    for (std::size_t index = 0; index < component.constraints.size(); ++index) {
        if (index != 0) output << ',';
        write_string(output, component.constraints[index]);
    }
    output << "],\"asset\":";
    write_string(output, component.asset);
    output << ",\"confidence\":" << component.confidence << ",\"properties\":{";
    bool first = true;
    for (const auto& [key, value] : component.properties) {
        if (!first) output << ',';
        first = false;
        write_string(output, key);
        output << ':';
        write_string(output, value);
    }
    output << "},\"children\":[";
    for (std::size_t index = 0; index < component.children.size(); ++index) {
        if (index != 0) output << ',';
        write_component(output, component.children[index]);
    }
    output << "]}";
}

void validate_component(const Component& component, int width, int height,
                        std::set<std::string>& identifiers, std::vector<std::string>& errors) {
    if (component.id.empty()) errors.emplace_back("Component identifiers must not be empty.");
    else if (!identifiers.insert(component.id).second) errors.emplace_back("Component identifiers must be unique.");
    if (component.type.empty()) errors.emplace_back("Component types must not be empty.");
    if (component.bounds.width <= 0 || component.bounds.height <= 0)
        errors.emplace_back("Component bounds must have positive width and height.");
    if (component.bounds.x < 0 || component.bounds.y < 0 ||
        static_cast<std::int64_t>(component.bounds.x) + component.bounds.width > width ||
        static_cast<std::int64_t>(component.bounds.y) + component.bounds.height > height)
        errors.emplace_back("Component bounds must fit within the document dimensions.");
    if (!std::isfinite(component.confidence) || component.confidence < 0.0 || component.confidence > 1.0)
        errors.emplace_back("Component confidence must be a finite value between 0 and 1.");
    if (component.layout.gap < 0 || component.layout.columns < 0)
        errors.emplace_back("Layout gap and column count must not be negative.");
    if (component.style.border_width < 0 || component.style.border_radius < 0)
        errors.emplace_back("Style border width and radius must not be negative.");
    for (const auto& child : component.children)
        validate_component(child, width, height, identifiers, errors);
}

} // namespace

std::string layout_kind_name(LayoutKind kind) {
    switch (kind) {
    case LayoutKind::horizontal: return "horizontal";
    case LayoutKind::vertical: return "vertical";
    case LayoutKind::grid: return "grid";
    case LayoutKind::absolute: return "absolute";
    }
    return "absolute";
}

std::vector<std::string> validate(const Document& document) {
    std::vector<std::string> errors;
    if (document.schema_version != ir_schema_version)
        errors.emplace_back("Unsupported IR schema version.");
    if (document.width <= 0 || document.height <= 0)
        errors.emplace_back("Document dimensions must be positive.");
    if (document.layout.gap < 0 || document.layout.columns < 0)
        errors.emplace_back("Document layout gap and column count must not be negative.");
    std::set<std::string> identifiers;
    for (const auto& component : document.components)
        validate_component(component, document.width, document.height, identifiers, errors);
    return errors;
}

std::string to_json(const Document& document) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{\"schema_version\":" << document.schema_version << ",\"document\":{\"width\":"
           << document.width << ",\"height\":" << document.height << ",\"metadata\":{";
    bool first = true;
    for (const auto& [key, value] : document.metadata) {
        if (!first) output << ',';
        first = false;
        write_string(output, key);
        output << ':';
        write_string(output, value);
    }
    output << "},\"layout\":";
    write_layout(output, document.layout);
    output << "},\"components\":[";
    for (std::size_t index = 0; index < document.components.size(); ++index) {
        if (index != 0) output << ',';
        write_component(output, document.components[index]);
    }
    output << "]}";
    return output.str();
}

} // namespace gradeant
