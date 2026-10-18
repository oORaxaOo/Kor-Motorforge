#include "core/scene_preview.hpp"

#include "core/runtime.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace motorforge {
namespace {
using Json = nlohmann::json;

ScenePreviewResult fail(std::string code, std::string message) {
    return {false, message, Json{{"contract_version","1"},{"ok",false},
        {"errors", Json::array({Json{{"code",code},{"message",message}}})}}.dump(2)};
}

bool read_text(const std::filesystem::path& path, std::string& value) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (in.bad()) return false;
    value = buffer.str();
    return true;
}

std::string base64(std::string_view input) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);
    for (std::size_t i=0; i<input.size(); i+=3) {
        const unsigned a=static_cast<unsigned char>(input[i]);
        const unsigned b=i+1<input.size()?static_cast<unsigned char>(input[i+1]):0U;
        const unsigned c=i+2<input.size()?static_cast<unsigned char>(input[i+2]):0U;
        const unsigned triple=(a<<16U)|(b<<8U)|c;
        out.push_back(alphabet[(triple>>18U)&63U]);
        out.push_back(alphabet[(triple>>12U)&63U]);
        out.push_back(i+1<input.size()?alphabet[(triple>>6U)&63U]:'=');
        out.push_back(i+2<input.size()?alphabet[triple&63U]:'=');
    }
    return out;
}

std::string xml_escape(std::string_view input) {
    std::string out;
    for (char c : input) {
        switch(c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}
} // namespace

ScenePreviewResult render_scene_preview_svg(std::string_view project_path,
    std::string_view scene_id, std::string_view output_path, int width, int height) {
    if (output_path.empty()) return fail("OUTPUT_PATH_REQUIRED","An output file path is required.");
    if (width < 320 || width > 4096 || height < 240 || height > 4096)
        return fail("INVALID_PREVIEW_SIZE","Preview width must be 320..4096 and height 240..4096.");
    if (std::filesystem::path{output_path}.extension() != ".svg")
        return fail("UNSUPPORTED_OUTPUT_FORMAT","Scene previews currently export as SVG.");

    const auto runtime = inspect_runtime(project_path, scene_id);
    if (!runtime.ok) return {false, runtime.message, runtime.json};
    const Json snapshot = Json::parse(runtime.json);
    const auto root = std::filesystem::canonical(std::filesystem::path{project_path});
    const auto& result = snapshot.at("result");
    const auto& entities = result.at("entities");

    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
        << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << ' ' << height << "\">\n";
    svg << "<title>" << xml_escape(result.at("scene").at("name").get<std::string>()) << "</title>\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"#171b20\"/>\n";
    svg << "<defs><pattern id=\"grid\" width=\"32\" height=\"32\" patternUnits=\"userSpaceOnUse\"><path d=\"M 32 0 L 0 0 0 32\" fill=\"none\" stroke=\"#293039\" stroke-width=\"1\"/></pattern></defs>\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"url(#grid)\"/>\n";
    svg << "<line x1=\"0\" y1=\"" << height/2 << "\" x2=\"" << width << "\" y2=\"" << height/2 << "\" stroke=\"#4a535e\" stroke-width=\"1\"/>\n";
    svg << "<line x1=\"" << width/2 << "\" y1=\"0\" x2=\"" << width/2 << "\" y2=\"" << height << "\" stroke=\"#4a535e\" stroke-width=\"1\"/>\n";

    std::size_t drawn=0;
    for (const auto& entity : entities) {
        const auto& t=entity.at("transform");
        const double x=t.at("x").get<double>(), y=t.at("y").get<double>();
        const double sx=t.at("scale_x").get<double>(), sy=t.at("scale_y").get<double>();
        const double rotation=t.at("rotation").get<double>();
        const double px=width/2.0+x*24.0, py=height/2.0-y*24.0;
        if (!entity.at("has_sprite").get<bool>()) {
            svg << "<g transform=\"translate(" << px << ' ' << py << ")\"><circle r=\"5\" fill=\"#e0b85b\"/><path d=\"M -10 0 H 10 M 0 -10 V 10\" stroke=\"#e0b85b\"/><text x=\"9\" y=\"-9\" fill=\"#e8edf2\" font-family=\"sans-serif\" font-size=\"12\">" << xml_escape(entity.at("name").get<std::string>()) << "</text></g>\n";
            continue;
        }
        const std::filesystem::path relative=entity.at("sprite").at("asset").get<std::string>();
        std::error_code ec;
        const auto asset=std::filesystem::canonical(root/relative,ec);
        const auto rel=asset.lexically_relative(root);
        if(ec || rel.empty() || rel.is_absolute() || *rel.begin()=="..")
            return fail("ASSET_NOT_FOUND_OR_UNSAFE","Sprite asset became unavailable or escaped the project during preview.");
        std::string source;
        if (!read_text(asset,source)) return fail("ASSET_READ_FAILED","Could not read sprite asset for preview.");
        if (source.find("<svg") == std::string::npos || source.find("</svg>") == std::string::npos)
            return fail("INVALID_SVG_ASSET","Sprite asset does not contain a recognizable SVG document.");
        svg << "<g transform=\"translate(" << px << ' ' << py << ") rotate(" << -rotation << ") scale(" << sx << ' ' << sy << ") translate(-32 -32)\"><image width=\"64\" height=\"64\" href=\"data:image/svg+xml;base64," << base64(source) << "\"/><title>" << xml_escape(entity.at("name").get<std::string>()) << "</title></g>\n";
        ++drawn;
    }
    svg << "<g font-family=\"sans-serif\" fill=\"#e8edf2\"><text x=\"18\" y=\"28\" font-size=\"16\" font-weight=\"bold\">KOR MOTORFORGE / SCENE PREVIEW</text><text x=\"18\" y=\"" << height-18 << "\" font-size=\"12\">" << xml_escape(result.at("scene").at("name").get<std::string>()) << " · " << entities.size() << " entities · " << drawn << " sprites · 24 px/world unit</text></g>\n</svg>\n";

    const std::filesystem::path destination{output_path};
    std::error_code ec;
    if (std::filesystem::exists(destination,ec))
        return fail("OUTPUT_PATH_EXISTS","Preview destination already exists; refusing to overwrite it.");
    if (ec) return fail("OUTPUT_PATH_CHECK_FAILED","Could not inspect preview destination.");
    const auto parent=destination.has_parent_path()?destination.parent_path():std::filesystem::path{"."};
    std::filesystem::create_directories(parent,ec);
    if(ec) return fail("OUTPUT_DIRECTORY_CREATE_FAILED","Could not create preview output directory.");
    auto temporary=destination; temporary += ".motorforge-tmp";
    if(std::filesystem::exists(temporary,ec)) return fail("OUTPUT_TEMP_PATH_EXISTS","Preview temporary path already exists.");
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(!out) return fail("PREVIEW_WRITE_FAILED","Could not open temporary preview file.");
        out << svg.str(); out.close();
        if(!out) { std::filesystem::remove(temporary,ec); return fail("PREVIEW_WRITE_FAILED","Could not write complete preview."); }
    }
    std::filesystem::rename(temporary,destination,ec);
    if(ec) { std::filesystem::remove(temporary,ec); return fail("PREVIEW_COMMIT_FAILED","Could not finalize preview file."); }
    Json response{{"contract_version","1"},{"ok",true},{"result",{{"output_path",destination.generic_string()},{"format","svg"},{"width",width},{"height",height},{"entity_count",entities.size()},{"sprite_count",drawn},{"scene_id",scene_id}}}};
    return {true,"Rendered SVG scene preview.",response.dump(2)};
}
} // namespace motorforge
