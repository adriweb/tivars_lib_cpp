/*
 * Generate the effective TI-84 Evo token map.
 *
 * Evo is the successor to the CE and uses its own 16-bit token words. The CE
 * token encoding is the legacy format referenced by EvoFormat's conversion
 * mappings; Evo is not a CE Python Edition variant.
 *
 * This intentionally asks EvoFormat.cpp to detokenize every possible 16-bit
 * word. EvoTokens.inc is only one of several mapping sources used by that
 * code, so iterating that table alone would produce an incomplete font.
 */

#include "EvoFormat.h"
#include "json.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

using json = nlohmann::ordered_json;

namespace
{
    struct EvoTokenInfo
    {
        uint16_t value;
        const char* identifier;
    };

#include "EvoTokens.inc"

    std::string hex_word(uint16_t value)
    {
        std::ostringstream out;
        out << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
        return out.str();
    }

    std::string utf8_from_bmp(uint16_t codepoint)
    {
        if (codepoint < 0x80)
        {
            return std::string(1, static_cast<char>(codepoint));
        }
        if (codepoint < 0x800)
        {
            return {
                static_cast<char>(0xC0 | (codepoint >> 6)),
                static_cast<char>(0x80 | (codepoint & 0x3F)),
            };
        }
        return {
            static_cast<char>(0xE0 | (codepoint >> 12)),
            static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)),
            static_cast<char>(0x80 | (codepoint & 0x3F)),
        };
    }

    std::string visible_glyph_text(uint16_t value, const std::string& readable)
    {
        if (value == 0x0000) return "[EOS]";
        if (readable == " ") return "[space]";
        if (readable == "\n") return "[newline]";
        if (readable == "\r") return "[return]";
        if (readable == "\t") return "[tab]";
        return readable;
    }

    std::string category_for(uint16_t value, const std::string& identifier,
                             const std::string& source)
    {
        if (value == 0x0000 || value == 0xEFFF) return "Sentinel";
        if (source == "ucs2") return "Character";
        if (value < 0xE000 || (value >= 0xF000 && value <= 0xF061)) return "Character";
        if (identifier.find("VAR_") != std::string::npos ||
            identifier.find("LIST") != std::string::npos ||
            identifier.find("MATRIX") != std::string::npos ||
            identifier.find("MAT_") != std::string::npos)
        {
            return "Variable / data";
        }
        if (value < 0xE400) return "Editor / UI";
        if (value < 0xEA00) return "TI-BASIC";
        return "System / internal";
    }
}

int main(int argc, char** argv)
{
    std::unordered_map<uint16_t, std::string> namedTokens;
    for (const auto& token : evoTokenInfos)
    {
        namedTokens.emplace(token.value, token.identifier);
    }

    json root;
    root["schemaVersion"] = 1;
    root["description"] = "Effective 16-bit Evo tokens recognized by tivars_lib_cpp";
    root["tokens"] = json::array();

    for (uint32_t wideValue = 0; wideValue <= 0xFFFF; wideValue++)
    {
        const auto value = static_cast<uint16_t>(wideValue);
        const std::string hex = hex_word(value);
        const data_t raw = {
            static_cast<uint8_t>(value & 0xFF),
            static_cast<uint8_t>(value >> 8),
        };
        const std::string readable = tivars::EvoFormat::detokenize_evo_token_words(raw);
        const std::string unknownEscape = "\\u" + hex;

        // 0000 is the explicit end-of-stream token. Every other \uNNNN
        // result is EvoFormat's unknown-word fallback, not a recognized token.
        if (value != 0x0000 && readable == unknownEscape)
        {
            continue;
        }

        const auto named = namedTokens.find(value);
        const std::string identifier = named == namedTokens.end() ? "" : named->second;
        std::string source = "derived";
        if (value == 0x0000) source = "sentinel";
        else if (named != namedTokens.end()) source = "named table";
        else if (readable == utf8_from_bmp(value)) source = "ucs2";

        root["tokens"].push_back({
            {"value", value},
            {"hex", hex},
            {"identifier", identifier},
            {"readable", readable},
            {"glyphText", visible_glyph_text(value, readable)},
            {"source", source},
            {"category", category_for(value, identifier, source)},
        });
    }

    root["tokenCount"] = root["tokens"].size();
    if (argc > 1)
    {
        std::ofstream output(argv[1]);
        if (!output)
        {
            std::cerr << "Could not open output file: " << argv[1] << '\n';
            return 1;
        }
        output << std::setw(2) << root << '\n';
    }
    else
    {
        std::cout << std::setw(2) << root << '\n';
    }
    return 0;
}
