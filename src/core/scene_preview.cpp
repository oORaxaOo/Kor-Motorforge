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
            svg << "<g data-entity-id=\"" << xml_escape(entity.at("id").get<std::string>()) << "\" data-entity-name=\"" << xml_escape(entity.at("name").get<std::string>()) << "\" data-x=\"" << x << "\" data-y=\"" << y << "\" data-rotation=\"" << rotation << "\" data-scale-x=\"" << sx << "\" data-scale-y=\"" << sy << "\" transform=\"translate(" << px << ' ' << py << ")\"><circle r=\"5\" fill=\"#e0b85b\"/><path d=\"M -10 0 H 10 M 0 -10 V 10\" stroke=\"#e0b85b\"/><text x=\"9\" y=\"-9\" fill=\"#e8edf2\" font-family=\"sans-serif\" font-size=\"12\">" << xml_escape(entity.at("name").get<std::string>()) << "</text></g>\n";
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

ScenePreviewResult render_interactive_scene_preview_html(std::string_view project_path,
    std::string_view scene_id, std::string_view output_path, int width, int height) {
    if (output_path.empty()) return fail("OUTPUT_PATH_REQUIRED", "An output file path is required.");
    if (std::filesystem::path{output_path}.extension() != ".html")
        return fail("UNSUPPORTED_OUTPUT_FORMAT", "Interactive scene previews must use the .html extension.");

    const std::filesystem::path destination{output_path};
    std::error_code ec;
    if (std::filesystem::exists(destination, ec))
        return fail("OUTPUT_PATH_EXISTS", "Preview destination already exists; refusing to overwrite it.");
    if (ec) return fail("OUTPUT_PATH_CHECK_FAILED", "Could not inspect preview destination.");

    auto source_path = destination;
    source_path += ".source.svg";
    if (std::filesystem::exists(source_path, ec))
        return fail("OUTPUT_TEMP_PATH_EXISTS", "Preview source temporary path already exists.");
    const auto rendered = render_scene_preview_svg(project_path, scene_id, source_path.string(), width, height);
    if (!rendered.ok) return rendered;

    std::string svg;
    const bool source_read = read_text(source_path, svg);
    std::filesystem::remove(source_path, ec);
    if (!source_read) return fail("PREVIEW_READ_FAILED", "Could not read the generated SVG preview.");


    std::ostringstream html;
    html << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
         << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         << "<title>Kor Motorforge — Interactive Scene Viewer</title><style>"
         << ":root{color-scheme:dark;font:14px system-ui,sans-serif;background:#101317;color:#e8edf2}"
         << "*{box-sizing:border-box}body{margin:0;height:100vh;display:grid;grid-template-rows:auto 1fr;overflow:hidden}"
         << "header{display:flex;align-items:center;gap:8px;padding:10px 14px;background:#1a1f26;border-bottom:1px solid #343c47;flex-wrap:wrap}"
         << "strong{letter-spacing:.08em;margin-right:auto}button{background:#29313b;color:#e8edf2;border:1px solid #46515f;border-radius:6px;padding:7px 11px;cursor:pointer}"
         << "button:hover{background:#374250}#viewport{position:relative;overflow:hidden;touch-action:none;background:#11151a;cursor:grab}"
         << "main{min-height:0}#workspace{display:grid;grid-template-columns:240px minmax(0,1fr);height:100%;min-height:0}"
         << "#sidebar{overflow:auto;background:#171b20;border-right:1px solid #343c47;padding:12px}h2{font-size:12px;text-transform:uppercase;letter-spacing:.1em;color:#aab5c2;margin:4px 0 10px}#hierarchy{display:grid;gap:4px}#hierarchy button{text-align:left;overflow-wrap:anywhere}#hierarchy button.selected{border-color:#e0b85b;background:#3b3322}#inspector{margin-top:22px;border-top:1px solid #343c47;padding-top:12px}dl{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:0}dt{color:#8f9baa}dd{margin:0;text-align:right;overflow-wrap:anywhere;font-variant-numeric:tabular-nums}#empty{color:#8f9baa;line-height:1.5}#viewport{position:relative;overflow:hidden;touch-action:none;background:#11151a;cursor:grab}#viewport.dragging{cursor:grabbing}#scene{position:absolute;left:0;top:0;width:100%;height:100%;transform-origin:0 0;user-select:none}#scene svg{display:block;width:100%;height:100%;overflow:visible}[data-entity-id]{cursor:pointer;outline:none}[data-entity-id].selected{filter:drop-shadow(0 0 5px #e0b85b)}#help{position:absolute;bottom:12px;left:12px;background:#171b20dd;color:#b8c2ce;padding:7px 10px;border-radius:5px;pointer-events:none}@media(max-width:650px){#workspace{grid-template-columns:155px minmax(0,1fr)}#sidebar{padding:8px}header{gap:5px;padding:8px}header strong{width:100%}}"
         << "</style></head><body><header><strong>KOR MOTORFORGE / INTERACTIVE SCENE VIEWER</strong>"
         << "<button id=\"minus\" aria-label=\"Zoom out\">−</button><button id=\"plus\" aria-label=\"Zoom in\">+</button>"
         << "<button id=\"reset\">Reset view</button><span id=\"zoom\">100%</span></header>"
         << "<main id=\"workspace\"><aside id=\"sidebar\"><h2>Scene hierarchy</h2><div id=\"hierarchy\"></div><section id=\"inspector\"><h2>Entity inspector</h2><div id=\"details\"><p id=\"empty\">Select an entity in the hierarchy or scene to inspect its properties.</p></div></section></aside><section id=\"viewport\" aria-label=\"Interactive scene preview\"><div id=\"scene\">" << svg << "</div>"
         << "<div id=\"help\">Select: click entity · Wheel: zoom · Drag: pan · 0: reset · +/−: zoom</div></section></main>"
         << "<script>(()=>{const v=document.getElementById('viewport'),i=document.getElementById('scene'),z=document.getElementById('zoom');"
         << "let scale=1,x=0,y=0,drag=null;const h=document.getElementById('hierarchy'),d=document.getElementById('details');"
         << "function paint(){i.style.transform='translate('+x+'px,'+y+'px) scale('+scale+')';z.textContent=Math.round(scale*100)+'%'}"
         << "function zoomAt(f,cx=v.clientWidth/2,cy=v.clientHeight/2){const next=Math.max(.2,Math.min(8,scale*f));const k=next/scale;x=cx-(cx-x)*k;y=cy-(cy-y)*k;scale=next;paint()}"
         << "function reset(){scale=1;x=0;y=0;paint()}document.getElementById('plus').onclick=()=>zoomAt(1.2);"
         << "document.getElementById('minus').onclick=()=>zoomAt(1/1.2);document.getElementById('reset').onclick=reset;"
         << "v.addEventListener('wheel',e=>{e.preventDefault();const r=v.getBoundingClientRect();zoomAt(e.deltaY<0?1.12:1/1.12,e.clientX-r.left,e.clientY-r.top)},{passive:false});"
         << "v.addEventListener('pointerdown',e=>{if(e.button!==0)return;drag={x:e.clientX-x,y:e.clientY-y,id:e.pointerId};v.setPointerCapture(e.pointerId);v.classList.add('dragging')});"
         << "v.addEventListener('pointermove',e=>{if(!drag)return;x=e.clientX-drag.x;y=e.clientY-drag.y;paint()});"
         << "function end(){drag=null;v.classList.remove('dragging')}v.addEventListener('pointerup',end);v.addEventListener('pointercancel',end);"
         << "function select(el){document.querySelectorAll('[data-entity-id]').forEach(n=>n.classList.toggle('selected',n===el));h.querySelectorAll('button').forEach(n=>n.classList.toggle('selected',n.dataset.id===el.dataset.entityId));d.replaceChildren();const title=document.createElement('p');title.textContent=el.dataset.entityName;d.appendChild(title);const dl=document.createElement('dl');for(const [label,key] of [['ID','entityId'],['Position X','x'],['Position Y','y'],['Rotation','rotation'],['Scale X','scaleX'],['Scale Y','scaleY']]){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=label;dd.textContent=el.dataset[key]??'—';dl.append(dt,dd)}d.appendChild(dl)}const entities=[...document.querySelectorAll('#scene [data-entity-id]')];entities.forEach(el=>{const button=document.createElement('button');button.type='button';button.textContent=el.dataset.entityName||el.dataset.entityId;button.dataset.id=el.dataset.entityId;button.onclick=()=>select(el);h.appendChild(button);el.addEventListener('click',e=>{e.stopPropagation();select(el)})});window.addEventListener('keydown',e=>{if(e.target.closest('button,input,textarea,select'))return;if(e.key==='0')reset();else if(e.key==='+'||e.key==='=')zoomAt(1.2);else if(e.key==='-')zoomAt(1/1.2)});paint()})();</script></body></html>\n";

    auto temporary = destination;
    temporary += ".motorforge-tmp";
    if (std::filesystem::exists(temporary, ec))
        return fail("OUTPUT_TEMP_PATH_EXISTS", "Preview temporary path already exists.");
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return fail("PREVIEW_WRITE_FAILED", "Could not open temporary HTML preview.");
        out << html.str();
        out.close();
        if (!out) {
            std::filesystem::remove(temporary, ec);
            return fail("PREVIEW_WRITE_FAILED", "Could not write complete HTML preview.");
        }
    }
    std::filesystem::rename(temporary, destination, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return fail("PREVIEW_COMMIT_FAILED", "Could not finalize HTML preview file.");
    }

    const Json svg_result = Json::parse(rendered.json);
    Json response{{"contract_version","1"},{"ok",true},{"result",{
        {"output_path",destination.generic_string()},{"format","html"},{"width",width},{"height",height},
        {"entity_count",svg_result["result"]["entity_count"]},
        {"sprite_count",svg_result["result"]["sprite_count"]},{"scene_id",scene_id},
        {"controls",Json::array({"zoom","pan","reset"})}}}};
    return {true, "Rendered interactive HTML scene viewer.", response.dump(2)};
}

} // namespace motorforge
