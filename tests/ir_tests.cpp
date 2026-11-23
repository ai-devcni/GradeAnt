#include "generation.hpp"
#include "ir.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

gradeant::Document sample_document() {
    gradeant::Document document;
    document.width = 320;
    document.height = 240;
    document.layout.kind = gradeant::LayoutKind::horizontal;
    document.layout.gap = 12;

    gradeant::Component panel;
    panel.id = "panel-1";
    panel.type = "container";
    panel.bounds = {10, 10, 140, 180};
    panel.confidence = 0.9;
    panel.layout.kind = gradeant::LayoutKind::horizontal;
    panel.layout.gap = 12;

    gradeant::Component button;
    button.id = "button-1";
    button.type = "button";
    button.bounds = {20, 20, 100, 40};
    button.text = "<Save & continue>";
    button.confidence = 0.8;
    panel.children.push_back(button);
    document.components.push_back(panel);

    gradeant::Component second_panel;
    second_panel.id = "panel-2";
    second_panel.bounds = {170, 10, 140, 180};
    second_panel.confidence = 0.75;
    document.components.push_back(second_panel);
    return document;
}

} // namespace

int main(int argc, char** argv) {
    try {
        auto document = sample_document();
        if (argc == 2 && std::string(argv[1]) == "--emit-ir") {
            std::cout << gradeant::to_json(document) << '\n';
            return 0;
        }
        require(argc == 1, "Unknown test argument.");
        require(gradeant::validate(document).empty(), "A valid document should pass validation.");

        const auto json = gradeant::to_json(document);
        require(json.find("\"schema_version\":1") != std::string::npos, "IR JSON must include schema version.");
        require(json.find("\"children\":[{") != std::string::npos, "IR JSON must preserve nested components.");
        require(json.find("<Save") != std::string::npos, "IR text is preserved in JSON.");

        const auto fidelity = gradeant::generate_html(document, gradeant::ReconstructionProfile::fidelity);
        require(fidelity.find("position: absolute;") != std::string::npos, "Fidelity output should preserve geometry.");
        require(fidelity.find("&lt;Save &amp; continue&gt;") != std::string::npos,
                "Generated text must be HTML escaped.");
        require(fidelity.find("<section class=") < fidelity.find("<button class="),
                "Generated markup must nest child components.");

        const auto hybrid = gradeant::generate_html(document, gradeant::ReconstructionProfile::hybrid);
        require(hybrid.find("display: flex;") != std::string::npos,
                "Hybrid output should use inferred layout relationships.");

        auto semantic_document = document;
        semantic_document.layout.kind = gradeant::LayoutKind::absolute;
        const auto semantic = gradeant::generate_html(
            semantic_document, gradeant::ReconstructionProfile::semantic);
        require(semantic.find("flex-direction: column;") != std::string::npos,
                "Semantic output should use a document-flow fallback for unclassified groups.");

        document.components.front().children.front().id = document.components.front().id;
        require(!gradeant::validate(document).empty(), "Duplicate component identifiers must fail validation.");
        std::cout << "IR and HTML generation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
