#import "TIVarsQuickLookSupport.h"

#include <algorithm>
#include <cctype>
#include <clocale>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "../src/EvoFormat.h"
#include "../src/TIFlashFile.h"
#include "../src/TIVarFile.h"
#include "../src/TIVarTypes.h"
#include "../src/TypeHandlers/TypeHandlers.h"
#include "../src/json.hpp"
#include "../src/tivarslib_utils.h"

using json = nlohmann::json;

namespace
{
    constexpr size_t kMaxPreviewChars = 32768;
    constexpr size_t kMaxPreviewArrayItems = 24;
    constexpr size_t kMaxPreviewJsonStringChars = 4096;
    constexpr size_t kMaxFlashExcerptBytes = 512;
    constexpr size_t kMaxEntryPreviewCount = 12;

    NSString* const errorDomain = @"com.adriweb.tivars-lib-cpp.quicklook";
    std::once_flag initFlag;

    NSString* nsstring_from_std(const std::string& value)
    {
        return [[NSString alloc] initWithBytes:value.data()
                                        length:value.size()
                                      encoding:NSUTF8StringEncoding];
    }

    std::string std_from_nsstring(NSString* value)
    {
        return value == nil ? std::string() : std::string(value.UTF8String ?: "");
    }

    std::string lowercase_ascii(std::string value)
    {
        for (char& c : value)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    std::string uppercase_ascii(std::string value)
    {
        for (char& c : value)
        {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return value;
    }

    std::string trim_nul_padded(const uint8_t* data, size_t size)
    {
        size_t length = 0;
        while (length < size && data[length] != '\0')
        {
            length++;
        }
        return std::string(reinterpret_cast<const char*>(data), length);
    }

    std::string html_escape(const std::string& input)
    {
        std::string escaped;
        escaped.reserve(input.size());
        for (const char c : input)
        {
            switch (c)
            {
                case '&': escaped += "&amp;"; break;
                case '<': escaped += "&lt;"; break;
                case '>': escaped += "&gt;"; break;
                case '"': escaped += "&quot;"; break;
                case '\'': escaped += "&#39;"; break;
                default: escaped.push_back(c); break;
            }
        }
        return escaped;
    }

    std::string file_name_for_path(const std::string& path)
    {
        const std::string::size_type slashPos = path.find_last_of('/');
        return slashPos == std::string::npos ? path : path.substr(slashPos + 1);
    }

    std::string extension_for_path(const std::string& path)
    {
        const std::string fileName = file_name_for_path(path);
        const std::string::size_type dotPos = fileName.find_last_of('.');
        return dotPos == std::string::npos ? std::string() : lowercase_ascii(fileName.substr(dotPos + 1));
    }

    bool is_flash_extension(const std::string& extension)
    {
        static const std::vector<std::string> flashExtensions = {
            "82u", "8xu", "8cu", "8eu", "8pu", "8yu",
            "8xk", "8ck", "8ek",
            "8xq", "8cq"
        };

        return std::find(flashExtensions.begin(), flashExtensions.end(), lowercase_ascii(extension)) != flashExtensions.end();
    }

    size_t file_size_for_path(const std::string& path)
    {
        struct stat fileStats{};
        return stat(path.c_str(), &fileStats) == 0 ? static_cast<size_t>(fileStats.st_size) : 0;
    }

    std::string format_size(size_t size)
    {
        static const char* units[] = {"B", "KB", "MB", "GB"};
        double value = static_cast<double>(size);
        size_t unitIndex = 0;
        while (value >= 1024.0 && unitIndex + 1 < (sizeof(units) / sizeof(units[0])))
        {
            value /= 1024.0;
            unitIndex++;
        }

        std::ostringstream out;
        if (unitIndex == 0)
        {
            out << static_cast<size_t>(value) << ' ' << units[unitIndex];
        }
        else
        {
            out << std::fixed << std::setprecision(value >= 100.0 ? 0 : 1) << value << ' ' << units[unitIndex];
        }
        return out.str();
    }

    std::string format_hex_byte(uint8_t value)
    {
        std::ostringstream out;
        out << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value);
        return out.str();
    }

    std::string display_model_name(const std::string& modelName)
    {
        static const std::vector<std::pair<std::string, std::string>> modelNames = {
            {"82", "TI-82"},
            {"83", "TI-83"},
            {"82A", "TI-82 Advanced"},
            {"84+T", "TI-84 Plus T"},
            {"83+", "TI-83 Plus"},
            {"82+", "TI-82 Plus"},
            {"84+", "TI-84 Plus"},
            {"84+CSE", "TI-84 Plus C Silver Edition"},
            {"84+CE", "TI-84 Plus CE"},
            {"84+CET", "TI-84 Plus CE-T"},
            {"84+CETPE", "TI-84 Plus CE-T Python Edition"},
            {"84+CEPy", "TI-84 Plus CE Python Edition"},
            {"83PCE", "TI-83 Premium CE"},
            {"83PCEEP", "TI-83 Premium CE Edition Python"},
            {"82AEP", "TI-82 Advanced Edition Python"},
            {"84Evo", "TI-84 Evo"},
        };

        const auto it = std::find_if(modelNames.begin(), modelNames.end(), [&](const auto& item) {
            return item.first == modelName;
        });
        return it == modelNames.end() ? modelName : it->second;
    }

    std::string data_to_hex_snippet(const data_t& data, size_t maxBytes)
    {
        if (data.empty())
        {
            return "";
        }

        const size_t limit = std::min(maxBytes, data.size());
        std::ostringstream out;
        for (size_t i = 0; i < limit; i++)
        {
            if (i > 0 && i % 16 == 0)
            {
                out << '\n';
            }
            else if (i > 0)
            {
                out << ' ';
            }
            out << tivars::dechex(data[i]);
        }
        if (limit < data.size())
        {
            out << "\n\n[preview truncated, " << (data.size() - limit) << " more bytes]";
        }
        return out.str();
    }

    std::string truncate_text(const std::string& text, size_t maxChars = kMaxPreviewChars)
    {
        if (text.size() <= maxChars)
        {
            return text;
        }

        std::ostringstream out;
        out << text.substr(0, maxChars);
        out << "\n\n[preview truncated, " << (text.size() - maxChars) << " more characters]";
        return out.str();
    }

    void sanitize_json_for_preview(json& value, const std::string& key = std::string(), size_t depth = 0)
    {
        if (depth > 12)
        {
            value = "[preview truncated: nested JSON omitted]";
            return;
        }

        if (value.is_object())
        {
            for (auto it = value.begin(); it != value.end(); ++it)
            {
                sanitize_json_for_preview(it.value(), it.key(), depth + 1);
            }
            return;
        }

        if (value.is_array())
        {
            if (value.size() > kMaxPreviewArrayItems)
            {
                const size_t truncatedItems = value.size() - kMaxPreviewArrayItems;
                value.erase(value.begin() + static_cast<json::difference_type>(kMaxPreviewArrayItems), value.end());
                value.push_back("[preview truncated: " + std::to_string(truncatedItems) + " more items]");
            }

            for (json& item : value)
            {
                sanitize_json_for_preview(item, std::string(), depth + 1);
            }
            return;
        }

        if (value.is_string())
        {
            const std::string str = value.get<std::string>();
            if (key == "readableContent")
            {
                try
                {
                    json nested = json::parse(str);
                    sanitize_json_for_preview(nested, key, depth + 1);
                    value = std::move(nested);
                    return;
                }
                catch (const std::exception&)
                {
                }
            }

            if (key == "previewImageDataUrl" && str.starts_with("data:image/"))
            {
                value = "[image rendered above]";
            }
            else if (key == "rawDataHex" && str.size() > 256)
            {
                value = str.substr(0, 256) + "... [preview truncated, " + std::to_string(str.size() / 2) + " bytes total]";
            }
            else if (str.size() > kMaxPreviewJsonStringChars)
            {
                const size_t omittedChars = str.size() - 256;
                value = str.substr(0, 256) + "... [preview truncated, " + std::to_string(omittedChars) + " more characters]";
            }
        }
    }

    std::string sanitize_readable_preview(const std::string& content)
    {
        try
        {
            json parsed = json::parse(content);
            sanitize_json_for_preview(parsed);
            return truncate_text(parsed.dump(2));
        }
        catch (const std::exception&)
        {
            return truncate_text(content);
        }
    }

    std::string preview_image_data_url_from_readable(const std::string& content, size_t depth = 0)
    {
        if (depth > 12)
        {
            return "";
        }

        try
        {
            const json parsed = json::parse(content);
            if (parsed.is_object() && parsed.contains("previewImageDataUrl") && parsed.at("previewImageDataUrl").is_string())
            {
                return parsed.at("previewImageDataUrl").get<std::string>();
            }

            if (parsed.is_object() && parsed.contains("readableContent") && parsed.at("readableContent").is_string())
            {
                return preview_image_data_url_from_readable(parsed.at("readableContent").get<std::string>(), depth + 1);
            }
        }
        catch (const std::exception&)
        {
        }

        return "";
    }

    NSImage* image_from_data_url(const std::string& dataUrl)
    {
        if (dataUrl.empty())
        {
            return nil;
        }

        const std::string::size_type commaPos = dataUrl.find(',');
        if (commaPos == std::string::npos)
        {
            return nil;
        }

        NSString* base64String = nsstring_from_std(dataUrl.substr(commaPos + 1));
        NSData* imageData = [[NSData alloc] initWithBase64EncodedString:base64String options:0];
        return imageData == nil ? nil : [[NSImage alloc] initWithData:imageData];
    }

    NSRect aspect_fit_rect(NSSize imageSize, NSRect bounds)
    {
        if (imageSize.width <= 0.0 || imageSize.height <= 0.0 || bounds.size.width <= 0.0 || bounds.size.height <= 0.0)
        {
            return bounds;
        }

        const CGFloat scale = std::min(bounds.size.width / imageSize.width, bounds.size.height / imageSize.height);
        const NSSize fittedSize = NSMakeSize(imageSize.width * scale, imageSize.height * scale);
        return NSMakeRect(bounds.origin.x + (bounds.size.width - fittedSize.width) * 0.5,
                          bounds.origin.y + (bounds.size.height - fittedSize.height) * 0.5,
                          fittedSize.width,
                          fittedSize.height);
    }

    std::string format_flash_date(const tivars::TIFlashFile::flash_date_t& date)
    {
        if (date.day == 0 && date.month == 0 && date.year == 0)
        {
            return "Unknown";
        }

        std::ostringstream out;
        out << std::setw(2) << std::setfill('0') << static_cast<int>(date.day)
            << '/'
            << std::setw(2) << std::setfill('0') << static_cast<int>(date.month)
            << '/'
            << date.year;
        return out.str();
    }

    std::string join_flash_devices(const std::vector<std::pair<uint8_t, uint8_t>>& devices)
    {
        if (devices.empty())
        {
            return "None";
        }

        std::ostringstream out;
        for (size_t i = 0; i < devices.size(); i++)
        {
            if (i > 0)
            {
                out << ", ";
            }
            out << "device " << format_hex_byte(devices[i].first)
                << " / type " << format_hex_byte(devices[i].second);
        }
        return out.str();
    }

    void append_meta_row(std::ostringstream& html, const std::string& key, const std::string& value)
    {
        html << "<div class=\"meta-key\">" << html_escape(key) << "</div>";
        html << "<div class=\"meta-value\">" << html_escape(value.empty() ? "-" : value) << "</div>";
    }

    void append_section(std::ostringstream& html, const std::string& title, const std::string& content)
    {
        html << "<section class=\"primary\">"
             << "<div class=\"section-label\">" << html_escape(title) << "</div>"
             << "<pre>" << html_escape(content.empty() ? "(empty)" : content) << "</pre>"
             << "</section>";
    }

    json primary_value_from_readable(const std::string& content)
    {
        json value;
        try
        {
            value = json::parse(content);
        }
        catch (const std::exception&)
        {
            return content;
        }

        if (value.is_object())
        {
            if (value.contains("readableContent"))
            {
                const json& readableContent = value.at("readableContent");
                if (readableContent.is_string())
                {
                    return primary_value_from_readable(readableContent.get<std::string>());
                }
                return readableContent;
            }
            if (value.contains("python") && value.at("python").is_object()
                && value.at("python").contains("code") && value.at("python").at("code").is_string())
            {
                return value.at("python").at("code");
            }
            if (value.contains("code") && value.at("code").is_string())
            {
                return value.at("code");
            }
        }

        return value;
    }

    bool type_name_contains(const std::string& typeName, const std::string& needle)
    {
        return lowercase_ascii(typeName).find(lowercase_ascii(needle)) != std::string::npos;
    }

    bool is_keyword(const std::string& word, bool python)
    {
        static const std::vector<std::string> pythonKeywords = {
            "and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del",
            "elif", "else", "except", "false", "finally", "for", "from", "global", "if", "import",
            "in", "is", "lambda", "none", "nonlocal", "not", "or", "pass", "raise", "return",
            "true", "try", "while", "with", "yield"
        };
        static const std::vector<std::string> tiBasicKeywords = {
            "and", "disp", "else", "end", "for", "get", "goto", "if", "input", "is", "lbl", "menu",
            "not", "or", "output", "pause", "prgm", "prompt", "repeat", "return", "send", "stop", "then",
            "to", "while", "xor"
        };

        const std::string normalized = lowercase_ascii(word);
        const auto& keywords = python ? pythonKeywords : tiBasicKeywords;
        return std::find(keywords.begin(), keywords.end(), normalized) != keywords.end();
    }

    size_t ti_basic_string_end(const std::string& source, size_t quotePos)
    {
        for (size_t pos = quotePos + 1; pos < source.size(); pos++)
        {
            if (source[pos] == '"')
            {
                return pos + 1;
            }
            if (source[pos] == '\n'
                || source.compare(pos, sizeof("→") - 1, "→") == 0
                || source.compare(pos, 2, "->") == 0)
            {
                return pos;
            }
        }
        return source.size();
    }

    std::string highlighted_source(const std::string& source, bool python)
    {
        std::ostringstream html;
        for (size_t pos = 0; pos < source.size();)
        {
            const char c = source[pos];
            if (c == '#')
            {
                const size_t end = source.find('\n', pos);
                const size_t length = end == std::string::npos ? source.size() - pos : end - pos;
                html << "<span class=\"tok-comment\">" << html_escape(source.substr(pos, length)) << "</span>";
                pos += length;
                continue;
            }

            if (!python && c == '"')
            {
                const size_t end = ti_basic_string_end(source, pos);
                html << "<span class=\"tok-string\">" << html_escape(source.substr(pos, end - pos)) << "</span>";
                pos = end;
                continue;
            }

            if (python && (c == '"' || c == '\''))
            {
                const char quote = c;
                size_t end = pos + 1;
                bool escaped = false;
                while (end < source.size())
                {
                    const char current = source[end++];
                    if (python && current == '\\' && !escaped)
                    {
                        escaped = true;
                        continue;
                    }
                    if (current == quote && !escaped)
                    {
                        break;
                    }
                    escaped = false;
                }
                html << "<span class=\"tok-string\">" << html_escape(source.substr(pos, end - pos)) << "</span>";
                pos = end;
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
            {
                size_t end = pos + 1;
                while (end < source.size()
                       && (std::isalnum(static_cast<unsigned char>(source[end])) || source[end] == '_'))
                {
                    end++;
                }
                const std::string word = source.substr(pos, end - pos);
                if (is_keyword(word, python))
                {
                    html << "<span class=\"tok-keyword\">" << html_escape(word) << "</span>";
                }
                else
                {
                    html << html_escape(word);
                }
                pos = end;
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                size_t end = pos + 1;
                while (end < source.size()
                       && (std::isdigit(static_cast<unsigned char>(source[end])) || source[end] == '.'))
                {
                    end++;
                }
                html << "<span class=\"tok-number\">" << html_escape(source.substr(pos, end - pos)) << "</span>";
                pos = end;
                continue;
            }

            html << html_escape(source.substr(pos, 1));
            pos++;
        }
        return html.str();
    }

    std::vector<std::string> split_values(const std::string& value, const std::string& separator)
    {
        std::vector<std::string> values;
        size_t start = 0;
        while (start <= value.size())
        {
            const size_t end = value.find(separator, start);
            values.push_back(tivars::trim(value.substr(start, end == std::string::npos ? std::string::npos : end - start)));
            if (end == std::string::npos)
            {
                break;
            }
            start = end + separator.size();
        }
        return values;
    }

    bool append_list_preview(std::ostringstream& html, const std::string& content)
    {
        const std::string trimmed = tivars::trim(content);
        if (trimmed.size() < 2 || trimmed.front() != '{' || trimmed.back() != '}')
        {
            return false;
        }

        const std::string inner = trimmed.substr(1, trimmed.size() - 2);
        const std::vector<std::string> values = inner.empty() ? std::vector<std::string>{} : split_values(inner, ",");
        html << "<section class=\"primary\"><div class=\"section-label\">Values</div>"
             << "<div class=\"table-scroll\"><table class=\"data-table list-table\"><thead><tr>"
             << "<th scope=\"col\">#</th><th scope=\"col\">Value</th></tr></thead><tbody>";
        const size_t count = std::min(values.size(), kMaxPreviewArrayItems);
        for (size_t index = 0; index < count; index++)
        {
            html << "<tr><th scope=\"row\">" << (index + 1) << "</th><td>" << html_escape(values[index]) << "</td></tr>";
        }
        html << "</tbody></table></div>";
        if (values.size() > count)
        {
            html << "<div class=\"preview-note\">" << (values.size() - count) << " more values omitted</div>";
        }
        if (values.empty())
        {
            html << "<div class=\"empty-state\">Empty list</div>";
        }
        html << "</section>";
        return true;
    }

    bool append_matrix_preview(std::ostringstream& html, const std::string& content)
    {
        const std::string trimmed = tivars::trim(content);
        if (trimmed.size() < 4 || !trimmed.starts_with("[[") || !trimmed.ends_with("]]"))
        {
            return false;
        }

        const std::vector<std::string> rows = split_values(trimmed.substr(2, trimmed.size() - 4), "][");
        if (rows.empty())
        {
            return false;
        }
        std::vector<std::vector<std::string>> cells;
        size_t columnCount = 0;
        for (const std::string& row : rows)
        {
            cells.push_back(split_values(row, ","));
            columnCount = std::max(columnCount, cells.back().size());
        }

        const size_t visibleRows = std::min(rows.size(), kMaxPreviewArrayItems);
        const size_t visibleColumns = std::min<size_t>(columnCount, 12);
        html << "<section class=\"primary\"><div class=\"section-label\">Matrix · "
             << rows.size() << " × " << columnCount << "</div>"
             << "<div class=\"table-scroll\"><table class=\"data-table matrix-table\"><thead><tr><th></th>";
        for (size_t column = 0; column < visibleColumns; column++)
        {
            html << "<th scope=\"col\">" << (column + 1) << "</th>";
        }
        html << "</tr></thead><tbody>";
        for (size_t row = 0; row < visibleRows; row++)
        {
            html << "<tr><th scope=\"row\">" << (row + 1) << "</th>";
            for (size_t column = 0; column < visibleColumns; column++)
            {
                html << "<td>" << (column < cells[row].size() ? html_escape(cells[row][column]) : "-") << "</td>";
            }
            html << "</tr>";
        }
        html << "</tbody></table></div>";
        if (rows.size() > visibleRows || columnCount > visibleColumns)
        {
            html << "<div class=\"preview-note\">Showing " << visibleRows << " of " << rows.size()
                 << " rows and " << visibleColumns << " of " << columnCount << " columns</div>";
        }
        html << "</section>";
        return true;
    }

    void append_image_facts(std::ostringstream& html, const json& primaryValue)
    {
        if (!primaryValue.is_object())
        {
            return;
        }

        html << "<div class=\"fact-row\">";
        if (primaryValue.contains("width") && primaryValue.contains("height"))
        {
            html << "<span class=\"fact\">" << html_escape(primaryValue.at("width").dump()) << " × "
                 << html_escape(primaryValue.at("height").dump()) << " px</span>";
        }
        if (primaryValue.contains("storage") && primaryValue.at("storage").is_object()
            && primaryValue.at("storage").contains("encoding"))
        {
            const json& encoding = primaryValue.at("storage").at("encoding");
            html << "<span class=\"fact\">" << html_escape(encoding.is_string() ? encoding.get<std::string>() : encoding.dump()) << "</span>";
        }
        if (primaryValue.contains("hasAlpha") && primaryValue.at("hasAlpha").is_boolean()
            && primaryValue.at("hasAlpha").get<bool>())
        {
            html << "<span class=\"fact\">Transparency</span>";
        }
        html << "</div>";
    }

    bool append_object_preview(std::ostringstream& html, const json& value)
    {
        if (!value.is_object())
        {
            return false;
        }

        if (value.contains("entries") && value.at("entries").is_array())
        {
            const json& entries = value.at("entries");
            html << "<section class=\"primary\"><div class=\"section-label\">Contents · " << entries.size() << "</div>"
                 << "<div class=\"table-scroll\"><table class=\"data-table contents-table\"><thead><tr>"
                 << "<th scope=\"col\">Name</th><th scope=\"col\">Type</th><th scope=\"col\">Preview</th>"
                 << "</tr></thead><tbody>";
            const size_t count = std::min(entries.size(), kMaxPreviewArrayItems);
            for (size_t index = 0; index < count; index++)
            {
                const json& entry = entries.at(index);
                const std::string name = entry.is_object() && entry.contains("name") && entry.at("name").is_string()
                    ? entry.at("name").get<std::string>() : "(unnamed)";
                const std::string type = entry.is_object() && entry.contains("typeName") && entry.at("typeName").is_string()
                    ? entry.at("typeName").get<std::string>() : "Unknown";
                std::string preview = "Structured data";
                if (entry.is_object() && entry.contains("readableContent"))
                {
                    const json& readable = entry.at("readableContent");
                    preview = readable.is_string() ? readable.get<std::string>() : "Structured data";
                }
                preview = truncate_text(preview, 100);
                html << "<tr><th scope=\"row\">" << html_escape(name) << "</th><td>" << html_escape(type)
                     << "</td><td>" << html_escape(preview) << "</td></tr>";
            }
            html << "</tbody></table></div>";
            if (entries.size() > count)
            {
                html << "<div class=\"preview-note\">" << (entries.size() - count) << " more entries omitted</div>";
            }
            html << "</section>";
            return true;
        }

        static const std::vector<std::string> hiddenKeys = {
            "metaData", "name", "previewImageDataUrl", "rawDataHex", "readableContent", "type", "typeName"
        };
        std::ostringstream rows;
        size_t visibleCount = 0;
        for (auto it = value.begin(); it != value.end() && visibleCount < kMaxPreviewArrayItems; ++it)
        {
            if (std::find(hiddenKeys.begin(), hiddenKeys.end(), it.key()) != hiddenKeys.end())
            {
                continue;
            }
            if (!(it.value().is_primitive() || (it.value().is_array() && it.value().size() <= 8)))
            {
                continue;
            }
            std::string renderedValue = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
            rows << "<div class=\"property\"><div class=\"property-key\">" << html_escape(it.key())
                 << "</div><div class=\"property-value\">" << html_escape(renderedValue) << "</div></div>";
            visibleCount++;
        }

        if (visibleCount == 0)
        {
            return false;
        }
        html << "<section class=\"primary\"><div class=\"section-label\">Values</div>"
             << "<div class=\"property-grid\">" << rows.str() << "</div></section>";
        return true;
    }

    void append_primary_content(std::ostringstream& html,
                                const std::string& entryName,
                                const std::string& typeName,
                                const std::string& asIsReadableContent,
                                const std::string& prettyReadableContent,
                                const std::string& previewImageDataUrl,
                                size_t entryIndex)
    {
        const json primaryValue = primary_value_from_readable(prettyReadableContent);
        const json asIsPrimaryValue = primary_value_from_readable(asIsReadableContent);
        if (!previewImageDataUrl.empty())
        {
            html << "<section class=\"primary image-primary\"><div class=\"section-label\">Preview</div>"
                 << "<div class=\"image-frame\"><img class=\"preview-image\" src=\"" << html_escape(previewImageDataUrl)
                 << "\" alt=\"Preview of " << html_escape(entryName) << "\"></div>";
            append_image_facts(html, primaryValue);
            html << "</section>";
            return;
        }

        if (primaryValue.is_string())
        {
            const std::string content = primaryValue.get<std::string>();
            if (type_name_contains(typeName, "matrix") && append_matrix_preview(html, content))
            {
                return;
            }
            if (type_name_contains(typeName, "list") && append_list_preview(html, content))
            {
                return;
            }
            if (type_name_contains(typeName, "program") || type_name_contains(typeName, "python")
                || type_name_contains(typeName, "equation"))
            {
                const bool python = type_name_contains(typeName, "python");
                const bool tiBasic = !python;
                const std::string asIsContent = asIsPrimaryValue.is_string()
                    ? asIsPrimaryValue.get<std::string>() : content;
                html << "<section class=\"primary\"><div class=\"section-label\">"
                     << (type_name_contains(typeName, "equation") ? "Expression" : "Source") << "</div>";
                if (tiBasic)
                {
                    const std::string suffix = std::to_string(entryIndex);
                    const std::string groupName = "source-format-" + suffix;
                    const std::string asIsId = "source-as-is-" + suffix;
                    const std::string prettyId = "source-pretty-" + suffix;
                    html << "<div class=\"source-format\" role=\"group\" aria-label=\"TI-BASIC formatting\">"
                         << "<input class=\"format-input format-as-is\" type=\"radio\" name=\"" << groupName
                         << "\" id=\"" << asIsId << "\"><label for=\"" << asIsId << "\">As-is</label>"
                         << "<input class=\"format-input format-pretty\" type=\"radio\" name=\"" << groupName
                         << "\" id=\"" << prettyId << "\" checked><label for=\"" << prettyId << "\">Pretty</label>"
                         << "<div class=\"source-views\">"
                         << "<pre class=\"source-code source-as-is\"><code>"
                         << highlighted_source(asIsContent.empty() ? "(empty)" : asIsContent, false)
                         << "</code></pre>"
                         << "<pre class=\"source-code source-pretty\"><code>"
                         << highlighted_source(content.empty() ? "(empty)" : content, false)
                         << "</code></pre></div></div>";
                }
                else
                {
                    html << "<pre class=\"source-code\"><code>"
                         << highlighted_source(content.empty() ? "(empty)" : content, true)
                         << "</code></pre>";
                }
                html << "</section>";
                return;
            }

            html << "<section class=\"primary value-primary\"><div class=\"section-label\">Value</div>"
                 << "<div class=\"value-card\">" << html_escape(content.empty() ? "(empty)" : content) << "</div></section>";
            return;
        }

        if (primaryValue.is_number() || primaryValue.is_boolean() || primaryValue.is_null())
        {
            html << "<section class=\"primary value-primary\"><div class=\"section-label\">Value</div>"
                 << "<div class=\"value-card\">" << html_escape(primaryValue.dump()) << "</div></section>";
            return;
        }

        if (append_object_preview(html, primaryValue))
        {
            return;
        }

        if (primaryValue.is_object())
        {
            html << "<section class=\"primary\"><div class=\"section-label\">Preview</div>"
                 << "<div class=\"empty-state content-unavailable\">No structured preview is available for this variable yet. "
                 << "Technical data remains available under Entry details.</div></section>";
            return;
        }

        html << "<section class=\"primary\"><div class=\"section-label\">Readable content</div><pre>"
             << html_escape(sanitize_readable_preview(prettyReadableContent)) << "</pre></section>";
    }

    void append_summary_chip(std::ostringstream& html, const std::string& label, const std::string& cssClass = std::string())
    {
        html << "<span class=\"summary-chip";
        if (!cssClass.empty())
        {
            html << ' ' << cssClass;
        }
        html << "\">" << html_escape(label) << "</span>";
    }

    std::string make_html_document(const std::string& fileTitle,
                                   const std::string& badge,
                                   const std::string& subtitle,
                                   const std::string& bodyHtml,
                                   const std::string& warning = std::string())
    {
        std::ostringstream html;
        html << "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
             << "<style>"
             << ":root{color-scheme:light dark;"
             << "--bg-top:#f8f3ea;--bg-bottom:#efe6d8;--panel:#fffdf9;--panel-subtle:#faf7f1;--ink:#1f2937;--muted:#5b6470;"
             << "--line:#d9d2c4;--line-soft:#ece5d7;--accent:#0f4c5c;--accent-soft:#d6e7ea;--accent-ink:#0b3b47;"
             << "--code:#10212c;--string:#9a3412;--keyword:#0369a1;--number:#7c3aed;--comment:#64748b;--warn:#9f1239;--warn-soft:#fde2e7;}"
             << "*{box-sizing:border-box;}"
             << "html,body{margin:0;padding:0;background:linear-gradient(180deg,var(--bg-top) 0%,var(--bg-bottom) 100%);font:15px/1.5 -apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;color:var(--ink);}"
             << "main{padding:20px 24px 30px;max-width:980px;margin:0 auto;}"
             << ".hero{display:flex;flex-wrap:wrap;align-items:center;gap:16px;background:radial-gradient(circle at top right,var(--accent-soft) 0%,var(--panel) 58%);border:1px solid var(--line);border-radius:18px;padding:16px 18px;box-shadow:0 8px 24px rgba(15,76,92,0.07);margin-bottom:12px;}"
             << ".hero-copy{min-width:0;flex:1;}"
             << ".badge{flex:none;display:inline-block;padding:8px 10px;border-radius:10px;background:var(--accent);color:#fff;font:700 12px/1 Menlo,Monaco,monospace;letter-spacing:.06em;text-transform:uppercase;}"
             << "h1{margin:0;font-size:25px;line-height:1.12;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}"
             << ".subtitle{margin-top:4px;color:var(--muted);font-size:15px;}"
             << ".warning{flex-basis:100%;margin-top:4px;padding:10px 12px;border-radius:12px;background:var(--warn-soft);color:var(--warn);font-weight:600;}"
             << ".summary-bar{display:flex;flex-wrap:wrap;gap:7px;margin:0 0 12px;padding:0 2px;}"
             << ".summary-chip,.fact{display:inline-flex;align-items:center;min-height:26px;padding:5px 9px;border-radius:999px;background:var(--accent-soft);color:var(--accent-ink);font-size:12px;font-weight:650;}"
             << ".summary-chip.status-ok{background:#dcfce7;color:#166534;}"
             << ".summary-chip.status-warn{background:var(--warn-soft);color:var(--warn);}"
             << ".grid{display:grid;grid-template-columns:max-content 1fr;gap:7px 18px;align-items:start;}"
             << ".meta-key{color:var(--muted);font-weight:600;}"
             << ".meta-value{word-break:break-word;}"
             << ".entries{display:grid;gap:14px;}"
             << ".entries>h2{margin:2px 2px -2px;font-size:17px;}"
             << ".entry{background:var(--panel);border:1px solid var(--line);border-radius:18px;padding:16px 18px;box-shadow:0 5px 18px rgba(31,41,55,0.04);}"
             << ".entry-header{display:flex;justify-content:space-between;gap:16px;align-items:flex-start;margin-bottom:12px;}"
             << ".entry-title{font-size:19px;font-weight:720;}"
             << ".entry-kind{color:var(--muted);margin-top:1px;}"
             << ".entry-size{font:700 12px/1 Menlo,Monaco,monospace;color:var(--accent-ink);background:var(--accent-soft);padding:6px 9px;border-radius:999px;white-space:nowrap;}"
             << ".primary{margin-top:2px;}"
             << ".section-label{margin:0 0 8px;font-size:12px;font-weight:750;text-transform:uppercase;letter-spacing:.055em;color:var(--muted);}"
             << ".image-frame{display:flex;justify-content:center;align-items:center;min-height:220px;max-height:520px;background-color:#efe6d8;background-image:linear-gradient(45deg,rgba(255,255,255,0.55) 25%,transparent 25%,transparent 75%,rgba(255,255,255,0.55) 75%,rgba(255,255,255,0.55)),linear-gradient(45deg,rgba(255,255,255,0.55) 25%,transparent 25%,transparent 75%,rgba(255,255,255,0.55) 75%,rgba(255,255,255,0.55));background-position:0 0,12px 12px;background-size:24px 24px;border:1px solid var(--line);border-radius:14px;padding:12px;overflow:hidden;}"
             << ".preview-image{display:block;width:100%;max-width:100%;max-height:490px;object-fit:contain;image-rendering:-webkit-optimize-contrast;image-rendering:pixelated;border-radius:8px;box-shadow:0 8px 22px rgba(17,24,39,0.14);}"
             << ".fact-row{display:flex;flex-wrap:wrap;gap:7px;margin-top:9px;}"
             << ".source-code{max-height:560px;overflow:auto;}"
             << ".source-format{display:flex;flex-wrap:wrap;gap:6px;align-items:center;}"
             << ".format-input{position:absolute;width:1px;height:1px;opacity:0;pointer-events:none;}"
             << ".source-format>label{cursor:pointer;padding:5px 10px;border:1px solid var(--line);border-radius:999px;color:var(--muted);font-size:12px;font-weight:700;background:var(--panel-subtle);}"
             << ".format-input:checked+label{border-color:var(--accent);background:var(--accent-soft);color:var(--accent-ink);}"
             << ".format-input:focus-visible+label{outline:2px solid var(--keyword);outline-offset:2px;}"
             << ".source-views{flex-basis:100%;min-width:0;margin-top:2px;}"
             << ".format-as-is:checked~.source-views .source-pretty,.format-pretty:checked~.source-views .source-as-is{display:none;}"
             << ".tok-string{color:var(--string);}.tok-keyword{color:var(--keyword);font-weight:700;}.tok-number{color:var(--number);}.tok-comment{color:var(--comment);font-style:italic;}"
             << ".value-card{padding:18px 20px;border:1px solid var(--line-soft);border-radius:14px;background:var(--panel-subtle);font:600 22px/1.35 Menlo,Monaco,monospace;overflow-wrap:anywhere;}"
             << ".table-scroll{overflow:auto;border:1px solid var(--line-soft);border-radius:14px;}"
             << ".data-table{width:100%;border-collapse:collapse;font:13px/1.4 Menlo,Monaco,monospace;background:var(--panel-subtle);}"
             << ".data-table th,.data-table td{padding:8px 11px;border-right:1px solid var(--line-soft);border-bottom:1px solid var(--line-soft);text-align:right;white-space:nowrap;}"
             << ".data-table tr:last-child th,.data-table tr:last-child td{border-bottom:0;}.data-table th:last-child,.data-table td:last-child{border-right:0;}"
             << ".data-table thead th,.data-table tbody th{background:var(--accent-soft);color:var(--accent-ink);font-weight:700;}"
             << ".list-table td{text-align:left;}"
             << ".contents-table th,.contents-table td{text-align:left;white-space:normal;}"
             << ".preview-note,.empty-state{margin-top:8px;color:var(--muted);font-size:13px;}"
             << ".content-unavailable{margin:0;padding:14px 16px;border:1px dashed var(--line);border-radius:13px;background:var(--panel-subtle);}"
             << ".property-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:8px;}"
             << ".property{padding:10px 12px;border:1px solid var(--line-soft);border-radius:12px;background:var(--panel-subtle);}"
             << ".property-key{color:var(--muted);font-size:12px;font-weight:650;}.property-value{margin-top:3px;font:600 14px/1.35 Menlo,Monaco,monospace;overflow-wrap:anywhere;}"
             << "details.details{margin-top:12px;border-top:1px solid var(--line-soft);padding-top:10px;}"
             << "details.file-details{margin-top:14px;padding:0;background:var(--panel);border:1px solid var(--line);border-radius:15px;overflow:hidden;}"
             << "details summary{cursor:pointer;color:var(--muted);font-weight:650;list-style-position:inside;}"
             << "details.file-details summary{padding:13px 16px;}"
             << ".details-body{padding:11px 2px 2px;}.file-details .details-body{padding:0 16px 16px;}"
             << ".raw-panel{margin-top:12px;}.raw-panel h3{margin:0 0 7px;font-size:12px;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);}"
             << "pre{margin:0;white-space:pre-wrap;word-break:break-word;font:13px/1.48 Menlo,Monaco,monospace;color:var(--code);background:var(--panel-subtle);padding:13px 15px;border-radius:13px;border:1px solid var(--line-soft);}"
             << "@media(prefers-color-scheme:dark){:root{--bg-top:#17191c;--bg-bottom:#101214;--panel:#202327;--panel-subtle:#181b1f;--ink:#f3f4f6;--muted:#aeb6c2;--line:#3b4148;--line-soft:#30363d;--accent:#2f8294;--accent-soft:#163d46;--accent-ink:#bfeaf2;--code:#e5edf5;--string:#fdba74;--keyword:#7dd3fc;--number:#c4b5fd;--comment:#94a3b8;--warn:#fda4af;--warn-soft:#4c1727;}.summary-chip.status-ok{background:#143d27;color:#86efac;}.image-frame{background-color:#2a2d31;background-image:linear-gradient(45deg,rgba(255,255,255,0.045) 25%,transparent 25%,transparent 75%,rgba(255,255,255,0.045) 75%,rgba(255,255,255,0.045)),linear-gradient(45deg,rgba(255,255,255,0.045) 25%,transparent 25%,transparent 75%,rgba(255,255,255,0.045) 75%,rgba(255,255,255,0.045));}}"
             << "@media(max-width:620px){main{padding:14px}.hero{padding:14px}.grid{grid-template-columns:1fr;gap:2px}.meta-value{margin-bottom:7px}.property-grid{grid-template-columns:1fr 1fr;}}"
             << "</style></head><body><main>"
             << "<section class=\"hero\">"
             << "<div class=\"badge\">" << html_escape(badge.empty() ? "TI" : badge) << "</div>"
             << "<div class=\"hero-copy\"><h1>" << html_escape(fileTitle) << "</h1>"
             << "<div class=\"subtitle\">" << html_escape(subtitle) << "</div></div>";

        if (!warning.empty())
        {
            html << "<div class=\"warning\">" << html_escape(warning) << "</div>";
        }

        html << "</section>" << bodyHtml << "</main></body></html>";
        return html.str();
    }

    std::string readable_entry_name(const tivars::TIVarFile::var_entry_t& entry, bool evoFormat)
    {
        if (evoFormat)
        {
            const std::string evoName = tivars::EvoFormat::decode_evo_name(entry.evoTypeID, entry.evoNameBytes);
            if (!evoName.empty())
            {
                return evoName;
            }
        }

        const std::string name = tivars::entry_name_to_string(entry._type, entry.varname, sizeof(entry.varname));
        return name.empty() ? "(unnamed)" : name;
    }

    std::string readable_entry_type(const tivars::TIVarFile::var_entry_t& entry, bool evoFormat)
    {
        return evoFormat
            ? tivars::EvoFormat::type_name_from_evo_type(entry.evoTypeID)
            : entry._type.getName();
    }

    struct RenderedDocument
    {
        std::string title;
        std::string subtitle;
        std::string badge;
        std::string warning;
        std::string html;
    };

    RenderedDocument render_var_document(const std::string& path)
    {
        const tivars::TIVarFile file = tivars::TIVarFile::loadFromFile(path);
        const auto& header = file.getHeader();
        const auto& entries = file.getVarEntries();
        const std::string fileName = file_name_for_path(path);
        const std::string extension = extension_for_path(path);
        const bool evoFormat = file.isEvoFormat();
        const std::string modelName = file.getCalcModel().getName();
        const std::string comment = tivars::trim(trim_nul_padded(header.comment, sizeof(header.comment)));

        std::ostringstream body;
        body << "<section class=\"summary-bar\" aria-label=\"File summary\">";
        append_summary_chip(body, display_model_name(modelName));
        append_summary_chip(body, std::to_string(entries.size()) + (entries.size() == 1 ? " entry" : " entries"));
        append_summary_chip(body, format_size(file_size_for_path(path)));
        append_summary_chip(body,
                            file.isCorrupt() ? "Checksum mismatch" : (evoFormat ? "Parsed successfully" : "Checksum valid"),
                            file.isCorrupt() ? "status-warn" : "status-ok");
        body << "</section>";

        body << "<section class=\"entries\">";
        if (entries.size() > 1)
        {
            body << "<h2>Entries</h2>";
        }
        const size_t previewCount = std::min(entries.size(), kMaxEntryPreviewCount);
        for (size_t index = 0; index < previewCount; index++)
        {
            const auto& entry = entries[index];
            std::string previewImageDataUrl;
            std::string asIsReadable;
            std::string prettyReadable;
            try
            {
                asIsReadable = file.getReadableContent({}, static_cast<uint16_t>(index));
                try
                {
                    prettyReadable = file.getReadableContent({{"prettify", true}, {"reindent", true}}, static_cast<uint16_t>(index));
                }
                catch (const std::exception&)
                {
                    prettyReadable = asIsReadable;
                }
                previewImageDataUrl = preview_image_data_url_from_readable(prettyReadable);
                if (previewImageDataUrl.empty())
                {
                    previewImageDataUrl = preview_image_data_url_from_readable(asIsReadable);
                }
            }
            catch (const std::exception& error)
            {
                asIsReadable = std::string("Preview unavailable: ") + error.what() + "\n\nRaw bytes:\n" + data_to_hex_snippet(entry.data, 256);
                prettyReadable = asIsReadable;
            }

            const std::string entryName = readable_entry_name(entry, evoFormat);
            const std::string entryType = readable_entry_type(entry, evoFormat);

            body << "<article class=\"entry\"><div class=\"entry-header\"><div>"
                 << "<div class=\"entry-title\">" << html_escape(entryName) << "</div>"
                 << "<div class=\"entry-kind\">" << html_escape(entryType) << "</div>"
                 << "</div><div class=\"entry-size\">" << html_escape(format_size(entry.data.size())) << "</div></div>";

            append_primary_content(body, entryName, entryType, asIsReadable, prettyReadable, previewImageDataUrl, index);

            body << "<details class=\"details\"><summary>Entry details</summary><div class=\"details-body\"><div class=\"grid\">";
            if (evoFormat)
            {
                append_meta_row(body, "Evo type ID", format_hex_byte(tivars::EvoFormat::evo_type_id_value(entry.evoTypeID)));
                append_meta_row(body, "Metadata version", std::to_string(entry.evoMetaVersion));
                append_meta_row(body, "Flags", format_hex_byte(entry.evoMetaFlags));
                append_meta_row(body, "Data encoding", entry.evoDataIsRawCBOR ? "CBOR value" : "Byte string");
            }
            else
            {
                append_meta_row(body, "Type ID", format_hex_byte(entry.typeID));
                append_meta_row(body, "Version", format_hex_byte(entry.version));
                append_meta_row(body, "Archived", entry.archivedFlag == 0x80 ? "Yes" : "No");
                append_meta_row(body, "Meta length", std::to_string(entry.meta_length));
            }
            append_meta_row(body, "Data length", std::to_string(evoFormat ? entry.data.size() : entry.data_length));
            body << "</div>";
            if (evoFormat || (!prettyReadable.empty() && (prettyReadable.front() == '{' || prettyReadable.front() == '[')))
            {
                body << "<section class=\"raw-panel\"><h3>Library output</h3><pre>"
                     << html_escape(sanitize_readable_preview(prettyReadable)) << "</pre></section>";
            }
            body << "</div></details>";
            body << "</article>";
        }

        if (entries.size() > previewCount)
        {
            body << "<div class=\"preview-note\">" << (entries.size() - previewCount) << " more entries omitted from Quick Look</div>";
        }
        body << "</section>";

        body << "<details class=\"details file-details\"><summary>File details</summary><div class=\"details-body\"><div class=\"grid\">";
        append_meta_row(body, "Container", evoFormat ? "TI Evo variable file (CBOR)" : "TI variable file");
        append_meta_row(body, "Calculator model", display_model_name(modelName));
        append_meta_row(body, "Entries", std::to_string(entries.size()));
        append_meta_row(body, "File size", format_size(file_size_for_path(path)));
        if (!evoFormat)
        {
            append_meta_row(body, "Owner PID", header.ownerPID == tivars::TIVarFile::OWNER_PID_NONE ? "None" : format_hex_byte(header.ownerPID));
            append_meta_row(body, "Signature", trim_nul_padded(header.signature, sizeof(header.signature)));
            append_meta_row(body, "Entries length", std::to_string(header.entries_len) + " bytes");
            append_meta_row(body, "Comment", comment.empty() ? "None" : comment);
        }
        body << "</div></div></details>";

        RenderedDocument rendered;
        rendered.title = fileName;
        rendered.subtitle = entries.empty()
            ? (evoFormat ? "Empty TI Evo variable file" : "Empty TI variable file")
            : entries.size() == 1
                ? readable_entry_type(entries.front(), evoFormat) + " • " + readable_entry_name(entries.front(), evoFormat)
                : std::string(evoFormat ? "Multi-entry TI Evo variable file • " : "Multi-entry TI variable file • ") + std::to_string(entries.size()) + " entries";
        rendered.badge = extension.empty() ? "TI" : extension;
        rendered.warning = file.isCorrupt() ? "Checksum mismatch detected. The file loaded, but it is flagged as corrupt." : "";
        rendered.html = make_html_document(rendered.title, rendered.badge, rendered.subtitle, body.str(), rendered.warning);
        return rendered;
    }

    RenderedDocument render_flash_document(const std::string& path)
    {
        const tivars::TIFlashFile file = tivars::TIFlashFile::loadFromFile(path);
        const auto& headers = file.getHeaders();
        const std::string fileName = file_name_for_path(path);
        const std::string extension = extension_for_path(path);

        std::ostringstream body;
        body << "<section class=\"summary-bar\" aria-label=\"File summary\">";
        append_summary_chip(body, "TI flash file");
        append_summary_chip(body, std::to_string(headers.size()) + (headers.size() == 1 ? " header" : " headers"));
        append_summary_chip(body, format_size(file_size_for_path(path)));
        body << "</section>";

        body << "<section class=\"entries\">";
        if (headers.size() > 1)
        {
            body << "<h2>Headers</h2>";
        }
        const size_t previewCount = std::min(headers.size(), kMaxEntryPreviewCount);
        for (size_t index = 0; index < previewCount; index++)
        {
            const auto& header = headers[index];
            body << "<article class=\"entry\"><div class=\"entry-header\"><div>"
                 << "<div class=\"entry-title\">" << html_escape(header.name.empty() ? "UNNAMED" : header.name) << "</div>"
                 << "<div class=\"entry-kind\">" << html_escape(header.type.getName()) << "</div>"
                 << "</div><div class=\"entry-size\">" << html_escape(format_size(header.calcData.size())) << "</div></div>";

            try
            {
                append_section(body, "Readable content", sanitize_readable_preview(file.getReadableContent(static_cast<uint16_t>(index))));
            }
            catch (const std::exception& error)
            {
                if (!header.calcData.empty())
                {
                    append_section(body, "Readable content", std::string("Preview unavailable: ") + error.what() + "\n\nRaw bytes:\n" + data_to_hex_snippet(header.calcData, kMaxFlashExcerptBytes));
                }
            }

            body << "<details class=\"details\"><summary>Header details</summary><div class=\"details-body\"><div class=\"grid\">";
            append_meta_row(body, "Model", display_model_name(header.model.getName()));
            append_meta_row(body, "Product ID", format_hex_byte(header.productId));
            append_meta_row(body, "Revision", header.revision);
            append_meta_row(body, "Date", format_flash_date(header.date));
            append_meta_row(body, "Binary flag", header.binaryFlag == tivars::TIFlashFile::rawBinaryDataFlag ? "Raw binary" : "Intel HEX");
            append_meta_row(body, "Object type", format_hex_byte(header.objectType));
            append_meta_row(body, "Devices", join_flash_devices(header.devices));
            append_meta_row(body, "Checksum", header.hasChecksum ? "Present" : "Missing");
            body << "</div></div></details>";

            body << "</article>";
        }

        if (headers.size() > previewCount)
        {
            body << "<div class=\"preview-note\">" << (headers.size() - previewCount) << " more headers omitted from Quick Look</div>";
        }
        body << "</section>";

        body << "<details class=\"details file-details\"><summary>File details</summary><div class=\"details-body\"><div class=\"grid\">";
        append_meta_row(body, "Container", "TI flash file");
        append_meta_row(body, "Headers", std::to_string(headers.size()));
        append_meta_row(body, "File size", format_size(file_size_for_path(path)));
        body << "</div></div></details>";

        RenderedDocument rendered;
        rendered.title = fileName;
        rendered.subtitle = headers.empty()
            ? "Empty TI flash file"
            : headers.size() == 1
                ? headers.front().type.getName() + " • " + (headers.front().name.empty() ? "UNNAMED" : headers.front().name)
                : "Multi-header TI flash file • " + std::to_string(headers.size()) + " headers";
        rendered.badge = extension.empty() ? "FLASH" : extension;
        rendered.html = make_html_document(rendered.title, rendered.badge, rendered.subtitle, body.str());
        return rendered;
    }

    RenderedDocument render_error_document(const std::string& path, const std::string& error)
    {
        std::ostringstream body;
        body << "<section class=\"meta\"><h2>Preview error</h2><div class=\"grid\">";
        append_meta_row(body, "File", file_name_for_path(path));
        append_meta_row(body, "Reason", error);
        body << "</div></section>";

        RenderedDocument rendered;
        rendered.title = file_name_for_path(path);
        rendered.subtitle = "Quick Look could not parse this file";
        rendered.badge = extension_for_path(path);
        rendered.warning = error;
        rendered.html = make_html_document(rendered.title, rendered.badge, rendered.subtitle, body.str(), rendered.warning);
        return rendered;
    }

    struct ThumbnailDescriptor
    {
        std::string badge;
        std::string title;
        std::string subtitle;
        std::string previewImageDataUrl;
        bool isFlash = false;
        bool isCorrupt = false;
    };

    ThumbnailDescriptor describe_for_thumbnail(const std::string& path)
    {
        ThumbnailDescriptor descriptor;
        descriptor.badge = extension_for_path(path);
        descriptor.title = "TI file";
        descriptor.subtitle = file_name_for_path(path);

        try
        {
            if (is_flash_extension(descriptor.badge))
            {
                const tivars::TIFlashFile flash = tivars::TIFlashFile::loadFromFile(path);
                const auto& headers = flash.getHeaders();
                descriptor.isFlash = true;
                descriptor.title = headers.empty() ? "Flash" : headers.front().type.getName();
                descriptor.subtitle = headers.empty() ? "TI flash file" : (headers.front().name.empty() ? "UNNAMED" : headers.front().name);
            }
            else
            {
                const tivars::TIVarFile var = tivars::TIVarFile::loadFromFile(path);
                const auto& entries = var.getVarEntries();
                const bool evoFormat = var.isEvoFormat();
                descriptor.isCorrupt = var.isCorrupt();
                descriptor.title = entries.empty() ? descriptor.title : readable_entry_name(entries.front(), evoFormat);
                descriptor.subtitle = entries.empty()
                    ? "Unknown content"
                    : readable_entry_type(entries.front(), evoFormat) + " · " + display_model_name(var.getCalcModel().getName());
                if (entries.size() == 1)
                {
                    descriptor.previewImageDataUrl = preview_image_data_url_from_readable(var.getReadableContent());
                }
            }
        }
        catch (const std::exception&)
        {
            descriptor.title = descriptor.badge.empty() ? "TI file" : "." + uppercase_ascii(descriptor.badge);
            descriptor.subtitle = "Unreadable";
        }

        if (descriptor.badge.empty())
        {
            descriptor.badge = descriptor.isFlash ? "FLASH" : "TI";
        }

        if (descriptor.title.size() > 26)
        {
            descriptor.title = descriptor.title.substr(0, 26);
        }
        if (descriptor.subtitle.size() > 26)
        {
            descriptor.subtitle = descriptor.subtitle.substr(0, 26);
        }

        descriptor.badge = uppercase_ascii(descriptor.badge);
        return descriptor;
    }

    void initialize_library()
    {
        std::call_once(initFlag, []() {
            std::setlocale(LC_ALL, ".UTF-8");
        });
    }

    void fill_rounded_rect(NSRect rect, CGFloat radius, NSColor* color)
    {
        [color setFill];
        [[NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius] fill];
    }

    void stroke_rounded_rect(NSRect rect, CGFloat radius, CGFloat width, NSColor* color)
    {
        [color setStroke];
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
        [path setLineWidth:width];
        [path stroke];
    }

    void draw_centered_string(NSString* string, NSRect rect, NSFont* font, NSColor* color)
    {
        if (string.length == 0)
        {
            return;
        }

        NSMutableParagraphStyle* paragraphStyle = [[NSMutableParagraphStyle alloc] init];
        paragraphStyle.alignment = NSTextAlignmentCenter;
        NSDictionary* attributes = @{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: color,
            NSParagraphStyleAttributeName: paragraphStyle,
        };
        [string drawInRect:rect withAttributes:attributes];
    }

    NSError* make_error(NSString* description)
    {
        return [NSError errorWithDomain:errorDomain
                                   code:1
                               userInfo:@{NSLocalizedDescriptionKey: description ?: @"Unknown Quick Look error"}];
    }

    BOOL draw_image_thumbnail(const ThumbnailDescriptor& descriptor, CGSize contextSize)
    {
        NSImage* previewImage = image_from_data_url(descriptor.previewImageDataUrl);
        if (previewImage == nil)
        {
            return NO;
        }

        const NSRect bounds = NSMakeRect(0.0, 0.0, contextSize.width, contextSize.height);
        [[NSColor colorWithCalibratedRed:0.96 green:0.93 blue:0.88 alpha:1.0] setFill];
        NSRectFill(bounds);

        const CGFloat margin = std::max<CGFloat>(12.0, contextSize.width * 0.06);
        const NSRect pageRect = NSInsetRect(bounds, margin, margin);
        fill_rounded_rect(pageRect, 18.0, [NSColor colorWithCalibratedRed:0.99 green:0.98 blue:0.96 alpha:1.0]);
        stroke_rounded_rect(pageRect, 18.0, 1.5, [NSColor colorWithCalibratedRed:0.84 green:0.81 blue:0.75 alpha:1.0]);

        const CGFloat footerHeight = std::max<CGFloat>(34.0, pageRect.size.height * 0.18);
        const NSRect imageBounds = NSInsetRect(NSMakeRect(pageRect.origin.x + 10.0,
                                                          pageRect.origin.y + footerHeight + 10.0,
                                                          pageRect.size.width - 20.0,
                                                          pageRect.size.height - footerHeight - 20.0), 4.0, 4.0);

        fill_rounded_rect(imageBounds, 12.0, [NSColor colorWithCalibratedRed:0.95 green:0.93 blue:0.89 alpha:1.0]);
        stroke_rounded_rect(imageBounds, 12.0, 1.0, [NSColor colorWithCalibratedRed:0.87 green:0.83 blue:0.77 alpha:1.0]);

        const NSRect fittedRect = aspect_fit_rect(previewImage.size, NSInsetRect(imageBounds, 8.0, 8.0));
        NSGraphicsContext* imageContext = NSGraphicsContext.currentContext;
        const NSImageInterpolation previousInterpolation = imageContext.imageInterpolation;
        imageContext.imageInterpolation = NSImageInterpolationNone;
        [previewImage drawInRect:fittedRect];
        imageContext.imageInterpolation = previousInterpolation;

        const NSRect footerRect = NSMakeRect(pageRect.origin.x,
                                             pageRect.origin.y,
                                             pageRect.size.width,
                                             footerHeight);
        fill_rounded_rect(footerRect, 18.0, [NSColor colorWithCalibratedRed:0.10 green:0.18 blue:0.26 alpha:0.92]);

        draw_centered_string(nsstring_from_std(descriptor.title),
                             NSMakeRect(footerRect.origin.x + 10.0,
                                        footerRect.origin.y + footerRect.size.height * 0.38,
                                        footerRect.size.width - 20.0,
                                        footerRect.size.height * 0.34),
                             [NSFont systemFontOfSize:std::max<CGFloat>(11.0, pageRect.size.width * 0.07) weight:NSFontWeightBold],
                             [NSColor whiteColor]);

        draw_centered_string(nsstring_from_std(descriptor.subtitle),
                             NSMakeRect(footerRect.origin.x + 12.0,
                                        footerRect.origin.y + footerRect.size.height * 0.10,
                                        footerRect.size.width - 24.0,
                                        footerRect.size.height * 0.24),
                             [NSFont systemFontOfSize:std::max<CGFloat>(9.0, pageRect.size.width * 0.042) weight:NSFontWeightMedium],
                             [NSColor colorWithCalibratedWhite:0.88 alpha:1.0]);

        return YES;
    }
}

@implementation TIVarsQuickLookSupport

+ (NSData *)previewHTMLDataForFileURL:(NSURL *)fileURL
                                title:(NSString * _Nullable * _Nullable)title
                                error:(NSError * _Nullable * _Nullable)error
{
    initialize_library();

    const std::string path = std_from_nsstring(fileURL.path);
    RenderedDocument rendered;

    try
    {
        rendered = is_flash_extension(extension_for_path(path))
            ? render_flash_document(path)
            : render_var_document(path);
    }
    catch (const std::exception& e)
    {
        rendered = render_error_document(path, e.what());
    }

    if (title != nullptr)
    {
        *title = nsstring_from_std(rendered.title);
    }

    NSData* data = [NSData dataWithBytes:rendered.html.data() length:rendered.html.size()];
    if (data == nil && error != nullptr)
    {
        *error = make_error(@"Failed to encode HTML preview data.");
    }
    return data;
}

+ (BOOL)drawThumbnailForFileURL:(NSURL *)fileURL
                    contextSize:(CGSize)contextSize
                          badge:(NSString * _Nullable * _Nullable)badge
                          error:(NSError * _Nullable * _Nullable)error
{
    initialize_library();

    const ThumbnailDescriptor descriptor = describe_for_thumbnail(std_from_nsstring(fileURL.path));
    if (badge != nullptr)
    {
        *badge = nsstring_from_std(descriptor.badge);
    }

    NSGraphicsContext* graphicsContext = NSGraphicsContext.currentContext;
    if (graphicsContext == nil)
    {
        if (error != nullptr)
        {
            *error = make_error(@"No active graphics context for thumbnail drawing.");
        }
        return NO;
    }

    if (!descriptor.previewImageDataUrl.empty() && draw_image_thumbnail(descriptor, contextSize))
    {
        return YES;
    }

    const NSRect bounds = NSMakeRect(0.0, 0.0, contextSize.width, contextSize.height);
    [[NSColor colorWithCalibratedRed:0.96 green:0.93 blue:0.88 alpha:1.0] setFill];
    NSRectFill(bounds);

    const CGFloat margin = std::max<CGFloat>(14.0, contextSize.width * 0.08);
    const NSRect pageRect = NSInsetRect(bounds, margin, margin);
    fill_rounded_rect(pageRect, 18.0, [NSColor colorWithCalibratedRed:0.99 green:0.98 blue:0.96 alpha:1.0]);
    stroke_rounded_rect(pageRect, 18.0, 1.5, [NSColor colorWithCalibratedRed:0.84 green:0.81 blue:0.75 alpha:1.0]);

    const CGFloat stripeHeight = std::max<CGFloat>(18.0, pageRect.size.height * 0.16);
    NSColor* stripeColor = descriptor.isCorrupt
        ? [NSColor colorWithCalibratedRed:0.75 green:0.16 blue:0.24 alpha:1.0]
        : (descriptor.isFlash
            ? [NSColor colorWithCalibratedRed:0.64 green:0.35 blue:0.18 alpha:1.0]
            : [NSColor colorWithCalibratedRed:0.06 green:0.30 blue:0.36 alpha:1.0]);
    fill_rounded_rect(NSMakeRect(pageRect.origin.x, pageRect.origin.y + pageRect.size.height - stripeHeight, pageRect.size.width, stripeHeight), 18.0, stripeColor);

    NSBezierPath* foldPath = [NSBezierPath bezierPath];
    const CGFloat foldWidth = pageRect.size.width * 0.18;
    const NSPoint topRight = NSMakePoint(NSMaxX(pageRect), NSMaxY(pageRect));
    [foldPath moveToPoint:NSMakePoint(topRight.x - foldWidth, topRight.y)];
    [foldPath lineToPoint:topRight];
    [foldPath lineToPoint:NSMakePoint(topRight.x, topRight.y - foldWidth)];
    [foldPath closePath];
    [[NSColor colorWithCalibratedRed:0.92 green:0.89 blue:0.82 alpha:1.0] setFill];
    [foldPath fill];

    NSString* badgeString = nsstring_from_std(descriptor.badge);
    NSString* titleString = nsstring_from_std(descriptor.title);
    NSString* subtitleString = nsstring_from_std(descriptor.subtitle);

    draw_centered_string(badgeString,
                         NSMakeRect(pageRect.origin.x + 12.0,
                                    pageRect.origin.y + pageRect.size.height * 0.43,
                                    pageRect.size.width - 24.0,
                                    pageRect.size.height * 0.22),
                         [NSFont monospacedSystemFontOfSize:std::max<CGFloat>(24.0, pageRect.size.width * 0.16) weight:NSFontWeightBold],
                         [NSColor colorWithCalibratedRed:0.10 green:0.18 blue:0.26 alpha:1.0]);

    draw_centered_string(titleString,
                         NSMakeRect(pageRect.origin.x + 12.0,
                                    pageRect.origin.y + pageRect.size.height * 0.21,
                                    pageRect.size.width - 24.0,
                                    pageRect.size.height * 0.12),
                         [NSFont systemFontOfSize:std::max<CGFloat>(13.0, pageRect.size.width * 0.075) weight:NSFontWeightBold],
                         [NSColor colorWithCalibratedRed:0.12 green:0.18 blue:0.23 alpha:1.0]);

    draw_centered_string(subtitleString,
                         NSMakeRect(pageRect.origin.x + 14.0,
                                    pageRect.origin.y + pageRect.size.height * 0.07,
                                    pageRect.size.width - 28.0,
                                    pageRect.size.height * 0.10),
                         [NSFont systemFontOfSize:std::max<CGFloat>(11.0, pageRect.size.width * 0.05) weight:NSFontWeightMedium],
                         [NSColor colorWithCalibratedRed:0.36 green:0.40 blue:0.45 alpha:1.0]);

    return YES;
}

@end
