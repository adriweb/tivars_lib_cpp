#import "TIVarsQuickLookSupport.h"

#include <algorithm>
#include <clocale>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "../src/TIFlashFile.h"
#include "../src/TIModels.h"
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
            const std::string& str = value.get_ref<const std::string&>();
            if (str.size() > kMaxPreviewJsonStringChars)
            {
                const size_t omittedChars = str.size() - 256;
                value = str.substr(0, 256) + "... [preview truncated, " + std::to_string(omittedChars) + " more characters]";
            }
            else if ((key == "rawDataHex" || key == "calcDataHex" || key == "dataHex") && str.size() > 128)
            {
                value = str.substr(0, 128) + "... [preview truncated, " + std::to_string(str.size() / 2) + " bytes total]";
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

    std::string model_name_for_header(const tivars::TIVarFile::var_header_t& header)
    {
        if (header.ownerPID != tivars::TIVarFile::OWNER_PID_NONE && tivars::TIModels::isValidPID(header.ownerPID))
        {
            return tivars::TIModels::fromPID(header.ownerPID).getName();
        }

        const std::string signature = trim_nul_padded(header.signature, sizeof(header.signature));
        if (!signature.empty() && tivars::TIModels::isValidSignature(signature))
        {
            return tivars::TIModels::fromSignature(signature).getName();
        }

        return "Unknown";
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
        html << "<section class=\"panel\">"
             << "<h3>" << html_escape(title) << "</h3>"
             << "<pre>" << html_escape(content.empty() ? "(empty)" : content) << "</pre>"
             << "</section>";
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
             << ":root{color-scheme:light;"
             << "--bg:#f4efe6;--panel:#fffdf9;--ink:#1f2937;--muted:#5b6470;"
             << "--line:#d9d2c4;--accent:#0f4c5c;--accent-soft:#d6e7ea;--warn:#9f1239;--warn-soft:#fde2e7;}"
             << "html,body{margin:0;padding:0;background:linear-gradient(180deg,#f8f3ea 0%,#efe6d8 100%);font:15px/1.55 -apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;color:var(--ink);}"
             << "main{padding:28px 32px 36px;max-width:980px;}"
             << ".hero{background:radial-gradient(circle at top right,#d7e7eb 0%,#fdfbf7 44%,#fffaf2 100%);border:1px solid var(--line);border-radius:20px;padding:24px 26px;box-shadow:0 12px 30px rgba(15,76,92,0.08);margin-bottom:22px;}"
             << ".badge{display:inline-block;padding:6px 10px;border-radius:999px;background:var(--accent);color:#fff;font:700 12px/1 Menlo,Monaco,monospace;letter-spacing:.08em;text-transform:uppercase;margin-bottom:14px;}"
             << "h1{margin:0;font-size:30px;line-height:1.1;}"
             << ".subtitle{margin-top:8px;color:var(--muted);font-size:16px;}"
             << ".warning{margin-top:16px;padding:12px 14px;border-radius:14px;background:var(--warn-soft);color:var(--warn);font-weight:600;}"
             << ".grid{display:grid;grid-template-columns:max-content 1fr;gap:8px 18px;align-items:start;}"
             << ".meta{background:var(--panel);border:1px solid var(--line);border-radius:18px;padding:18px 20px;margin-bottom:18px;}"
             << ".meta h2,.panel h2,.entries h2{margin:0 0 14px 0;font-size:18px;}"
             << ".meta-key{color:var(--muted);font-weight:600;}"
             << ".meta-value{word-break:break-word;}"
             << ".entries{display:grid;gap:18px;}"
             << ".entry{background:var(--panel);border:1px solid var(--line);border-radius:18px;padding:18px 20px;}"
             << ".entry-header{display:flex;justify-content:space-between;gap:16px;align-items:flex-start;margin-bottom:12px;}"
             << ".entry-title{font-size:20px;font-weight:700;}"
             << ".entry-kind{color:var(--muted);margin-top:3px;}"
             << ".entry-size{font:700 12px/1 Menlo,Monaco,monospace;color:var(--accent);background:var(--accent-soft);padding:6px 10px;border-radius:999px;white-space:nowrap;}"
             << ".panel{margin-top:14px;padding-top:14px;border-top:1px solid #ece5d7;}"
             << ".panel h3{margin:0 0 10px 0;font-size:14px;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);}"
             << "pre{margin:0;white-space:pre-wrap;word-break:break-word;font:13px/1.45 Menlo,Monaco,monospace;color:#10212c;background:#faf7f1;padding:14px 16px;border-radius:14px;border:1px solid #ede5d8;}"
             << "</style></head><body><main>"
             << "<section class=\"hero\">"
             << "<div class=\"badge\">" << html_escape(badge.empty() ? "TI" : badge) << "</div>"
             << "<h1>" << html_escape(fileTitle) << "</h1>"
             << "<div class=\"subtitle\">" << html_escape(subtitle) << "</div>";

        if (!warning.empty())
        {
            html << "<div class=\"warning\">" << html_escape(warning) << "</div>";
        }

        html << "</section>" << bodyHtml << "</main></body></html>";
        return html.str();
    }

    std::string readable_entry_name(const tivars::TIVarFile::var_entry_t& entry)
    {
        const std::string name = tivars::entry_name_to_string(entry._type, entry.varname, sizeof(entry.varname));
        return name.empty() ? "(unnamed)" : name;
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
        const std::string modelName = model_name_for_header(header);
        const std::string comment = tivars::trim(trim_nul_padded(header.comment, sizeof(header.comment)));

        std::ostringstream body;
        body << "<section class=\"meta\"><h2>File metadata</h2><div class=\"grid\">";
        append_meta_row(body, "Container", "TI variable file");
        append_meta_row(body, "Calculator model", modelName);
        append_meta_row(body, "Owner PID", header.ownerPID == tivars::TIVarFile::OWNER_PID_NONE ? "None" : format_hex_byte(header.ownerPID));
        append_meta_row(body, "Signature", trim_nul_padded(header.signature, sizeof(header.signature)));
        append_meta_row(body, "Entries", std::to_string(entries.size()));
        append_meta_row(body, "Entries length", std::to_string(header.entries_len) + " bytes");
        append_meta_row(body, "File size", format_size(file_size_for_path(path)));
        append_meta_row(body, "Comment", comment.empty() ? "None" : comment);
        body << "</div></section>";

        body << "<section class=\"entries\"><h2>Entries</h2>";
        const size_t previewCount = std::min(entries.size(), kMaxEntryPreviewCount);
        for (size_t index = 0; index < previewCount; index++)
        {
            const auto& entry = entries[index];
            std::string readable;
            try
            {
                options_t options;
                options["reindent"] = true;
                options["prettify"] = true;
                readable = sanitize_readable_preview(file.getReadableContent(options, static_cast<uint16_t>(index)));
            }
            catch (const std::exception& error)
            {
                readable = std::string("Preview unavailable: ") + error.what() + "\n\nRaw bytes:\n" + data_to_hex_snippet(entry.data, 256);
            }

            body << "<article class=\"entry\"><div class=\"entry-header\"><div>"
                 << "<div class=\"entry-title\">" << html_escape(readable_entry_name(entry)) << "</div>"
                 << "<div class=\"entry-kind\">" << html_escape(entry._type.getName()) << "</div>"
                 << "</div><div class=\"entry-size\">" << html_escape(format_size(entry.data.size())) << "</div></div>";

            body << "<section class=\"meta\"><div class=\"grid\">";
            append_meta_row(body, "Type ID", format_hex_byte(entry.typeID));
            append_meta_row(body, "Version", format_hex_byte(entry.version));
            append_meta_row(body, "Archived", entry.archivedFlag == 0x80 ? "Yes" : "No");
            append_meta_row(body, "Meta length", std::to_string(entry.meta_length));
            append_meta_row(body, "Data length", std::to_string(entry.data_length));
            body << "</div></section>";
            append_section(body, "Readable content", readable);
            body << "</article>";
        }

        if (entries.size() > previewCount)
        {
            body << "<section class=\"meta\"><div class=\"grid\">";
            append_meta_row(body, "Preview note", std::to_string(entries.size() - previewCount) + " more entries omitted from Quick Look");
            body << "</div></section>";
        }
        body << "</section>";

        RenderedDocument rendered;
        rendered.title = fileName;
        rendered.subtitle = entries.empty()
            ? "Empty TI variable file"
            : entries.size() == 1
                ? entries.front()._type.getName() + " • " + readable_entry_name(entries.front())
                : "Multi-entry TI variable file • " + std::to_string(entries.size()) + " entries";
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
        body << "<section class=\"meta\"><h2>File metadata</h2><div class=\"grid\">";
        append_meta_row(body, "Container", "TI flash file");
        append_meta_row(body, "Headers", std::to_string(headers.size()));
        append_meta_row(body, "File size", format_size(file_size_for_path(path)));
        body << "</div></section>";

        body << "<section class=\"entries\"><h2>Headers</h2>";
        const size_t previewCount = std::min(headers.size(), kMaxEntryPreviewCount);
        for (size_t index = 0; index < previewCount; index++)
        {
            const auto& header = headers[index];
            body << "<article class=\"entry\"><div class=\"entry-header\"><div>"
                 << "<div class=\"entry-title\">" << html_escape(header.name.empty() ? "UNNAMED" : header.name) << "</div>"
                 << "<div class=\"entry-kind\">" << html_escape(header.type.getName()) << "</div>"
                 << "</div><div class=\"entry-size\">" << html_escape(format_size(header.calcData.size())) << "</div></div>";

            body << "<section class=\"meta\"><div class=\"grid\">";
            append_meta_row(body, "Model", header.model.getName());
            append_meta_row(body, "Product ID", format_hex_byte(header.productId));
            append_meta_row(body, "Revision", header.revision);
            append_meta_row(body, "Date", format_flash_date(header.date));
            append_meta_row(body, "Binary flag", header.binaryFlag == tivars::TIFlashFile::rawBinaryDataFlag ? "Raw binary" : "Intel HEX");
            append_meta_row(body, "Object type", format_hex_byte(header.objectType));
            append_meta_row(body, "Devices", join_flash_devices(header.devices));
            append_meta_row(body, "Checksum", header.hasChecksum ? "Present" : "Missing");
            body << "</div></section>";

            if (header.binaryFlag == tivars::TIFlashFile::rawBinaryDataFlag && !header.calcData.empty())
            {
                append_section(body, "Raw data excerpt", data_to_hex_snippet(header.calcData, kMaxFlashExcerptBytes));
            }
            else if (!header.calcData.empty() && header.calcData.size() <= 4096)
            {
                append_section(body, "Readable content", sanitize_readable_preview(file.getReadableContent(index)));
            }

            body << "</article>";
        }

        if (headers.size() > previewCount)
        {
            body << "<section class=\"meta\"><div class=\"grid\">";
            append_meta_row(body, "Preview note", std::to_string(headers.size() - previewCount) + " more headers omitted from Quick Look");
            body << "</div></section>";
        }
        body << "</section>";

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
                descriptor.isCorrupt = var.isCorrupt();
                descriptor.badge = entries.empty() ? "Unknown var" : readable_entry_name(entries.front());
                descriptor.title = entries.empty() ? descriptor.title : entries.front()._type.getName();
                descriptor.subtitle = entries.empty() ? "Unknown content" : truncate_text(var.getReadableContent({{"prettify", true}}));
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
            tivars::TIModels::initTIModelsArray();
            tivars::TIVarTypes::initTIVarTypesArray();
            NSString* csvPath = [NSBundle.mainBundle pathForResource:@"programs_tokens" ofType:@"csv"];
            if (csvPath)
            {
                tivars::TypeHandlers::TH_Tokenized::initTokensFromCSVFilePath(csvPath.UTF8String);
            }
            else
            {
                tivars::TypeHandlers::TH_Tokenized::initTokens();
            }
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
