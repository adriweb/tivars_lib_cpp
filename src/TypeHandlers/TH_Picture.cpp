/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include "../json.hpp"
#include "../tivarslib_utils.h"

#include <cctype>
#include <stdexcept>

using json = nlohmann::json;

namespace tivars::TypeHandlers
{
    namespace
    {
        uint16_t read_le16(const data_t& data)
        {
            if (data.size() < 2)
            {
                throw std::invalid_argument("Invalid picture/image data. Needs at least 2 bytes");
            }
            return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
        }

        data_t parse_hex_string(const std::string& str, const char* fieldName)
        {
            if (str.size() % 2 != 0)
            {
                throw std::invalid_argument(std::string(fieldName) + " must contain an even number of hex digits");
            }

            for (const char c : str)
            {
                if (!std::isxdigit(static_cast<unsigned char>(c)))
                {
                    throw std::invalid_argument(std::string(fieldName) + " must be valid hexadecimal");
                }
            }

            data_t data;
            data.reserve(str.size() / 2);
            for (size_t i = 0; i < str.size(); i += 2)
            {
                data.push_back(hexdec(str.substr(i, 2)));
            }
            return data;
        }
    }

    data_t TH_Picture::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;

        const json j = json::parse(str);
        if (!j.is_object() || !j.contains("rawDataHex"))
        {
            throw std::runtime_error("Picture/Image string -> data requires a JSON object with rawDataHex");
        }

        data_t data = parse_hex_string(j.at("rawDataHex").get<std::string>(), "rawDataHex");
        const json parsed = json::parse(makeStringFromData(data, {}, _ctx));

        if (j.contains("typeName") && j.at("typeName").get<std::string>() != parsed.at("typeName").get<std::string>())
        {
            throw std::invalid_argument("rawDataHex does not match the requested picture/image type");
        }

        return data;
    }

    std::string TH_Picture::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        const size_t actualSize = data.size();
        const uint16_t storedLength = read_le16(data);
        if (actualSize != storedLength + minimumDataByteCount)
        {
            throw std::invalid_argument("Invalid picture/image data length");
        }

        json j;
        j["dataLength"] = actualSize;
        j["storedLength"] = storedLength;

        switch (actualSize)
        {
            case monoPictureDataByteCount:
                j["kind"] = "MonoPicture";
                j["typeName"] = "Picture";
                j["hasColor"] = false;
                j["width"] = monoPictureWidth;
                j["height"] = monoPictureHeight;
                j["storage"] = {
                    {"encoding", "L1"},
                    {"rowsStoredReversed", false},
                    {"pixelsPerByte", 8},
                    {"dataWidth", monoPictureWidth / 8},
                    {"dataHeight", monoPictureHeight},
                };
                break;

            case colorPictureDataByteCount:
                j["kind"] = "ColorPicture";
                j["typeName"] = "Picture";
                j["hasColor"] = true;
                j["width"] = colorPictureWidth;
                j["height"] = colorPictureHeight;
                j["storage"] = {
                    {"encoding", "RGBPalette"},
                    {"paletteSize", 15},
                    {"rowsStoredReversed", false},
                    {"pixelsPerByte", 2},
                    {"dataWidth", colorPictureWidth / 2},
                    {"dataHeight", colorPictureHeight},
                };
                break;

            case imageDataByteCount:
                if (data.size() < 3 || data[2] != imageMagic)
                {
                    throw std::invalid_argument("Invalid image data magic");
                }
                j["kind"] = "Image";
                j["typeName"] = "Image";
                j["hasColor"] = true;
                j["width"] = imageWidth;
                j["height"] = imageHeight;
                j["storage"] = {
                    {"encoding", "RGB565"},
                    {"rowsStoredReversed", true},
                    {"rowPaddingBytes", 2},
                    {"imageMagic", dechex(imageMagic)},
                    {"dataWidth", 2 * imageWidth + minimumDataByteCount},
                    {"dataHeight", imageHeight},
                };
                break;

            default:
                throw std::invalid_argument("Unsupported picture/image data size: " + std::to_string(actualSize));
        }

        return j.dump(4);
    }

    uint8_t TH_Picture::getMinVersionFromData(const data_t& data)
    {
        if (data.size() == colorPictureDataByteCount)
        {
            return 0x0A;
        }
        return 0x00;
    }
}
