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
        constexpr std::array<std::array<uint8_t, 3>, 16> colorPicturePalette = {{
            {{255, 255, 255}},
            {{0, 0, 255}},
            {{255, 0, 0}},
            {{0, 0, 0}},
            {{255, 0, 255}},
            {{0, 160, 0}},
            {{255, 165, 0}},
            {{165, 96, 42}},
            {{0, 0, 128}},
            {{96, 192, 255}},
            {{255, 255, 0}},
            {{255, 255, 255}},
            {{216, 216, 216}},
            {{176, 176, 176}},
            {{128, 128, 128}},
            {{72, 72, 72}},
        }};

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

        data_t make_mono_picture_preview_rgb(const data_t& data)
        {
            data_t rgb;
            rgb.reserve(TH_Picture::monoPictureWidth * TH_Picture::monoPictureHeight * 3);

            const size_t rowByteCount = TH_Picture::monoPictureWidth / 8;
            for (size_t y = 0; y < TH_Picture::monoPictureHeight; y++)
            {
                const size_t rowStart = TH_Picture::minimumDataByteCount + y * rowByteCount;
                for (size_t byteIdx = 0; byteIdx < rowByteCount; byteIdx++)
                {
                    const uint8_t packed = rowStart + byteIdx < data.size() ? data[rowStart + byteIdx] : 0;
                    for (int bit = 7; bit >= 0; bit--)
                    {
                        const bool isBlack = ((packed >> bit) & 1U) != 0;
                        const uint8_t channel = isBlack ? 0x00 : 0xFF;
                        rgb.push_back(channel);
                        rgb.push_back(channel);
                        rgb.push_back(channel);
                    }
                }
            }

            return rgb;
        }

        data_t make_color_picture_preview_rgb(const data_t& data)
        {
            data_t rgb;
            rgb.reserve(TH_Picture::colorPictureWidth * TH_Picture::colorPictureHeight * 3);

            const size_t rowByteCount = TH_Picture::colorPictureWidth / 2;
            for (size_t y = 0; y < TH_Picture::colorPictureHeight; y++)
            {
                const size_t rowStart = TH_Picture::minimumDataByteCount + y * rowByteCount;
                for (size_t byteIdx = 0; byteIdx < rowByteCount; byteIdx++)
                {
                    const uint8_t packed = rowStart + byteIdx < data.size() ? data[rowStart + byteIdx] : 0;
                    for (const uint8_t index : { static_cast<uint8_t>((packed >> 4) & 0x0F), static_cast<uint8_t>(packed & 0x0F) })
                    {
                        const auto& color = colorPicturePalette[index];
                        rgb.push_back(color[0]);
                        rgb.push_back(color[1]);
                        rgb.push_back(color[2]);
                    }
                }
            }

            return rgb;
        }

        data_t make_image_preview_rgb(const data_t& data)
        {
            data_t rgb(static_cast<size_t>(TH_Picture::imageWidth) * TH_Picture::imageHeight * 3, 0x00);
            const size_t rowStride = TH_Picture::imageWidth * 2 + TH_Picture::minimumDataByteCount;
            const size_t pixelStart = TH_Picture::minimumDataByteCount + 1;

            for (size_t storedRow = 0; storedRow < TH_Picture::imageHeight; storedRow++)
            {
                const size_t rowStart = pixelStart + storedRow * rowStride;
                if (rowStart >= data.size())
                {
                    break;
                }

                const size_t y = TH_Picture::imageHeight - 1 - storedRow;
                for (size_t x = 0; x < TH_Picture::imageWidth; x++)
                {
                    const size_t src = rowStart + x * 2;
                    if (src + 1 >= data.size())
                    {
                        break;
                    }

                    const auto color = rgb565_to_rgb888(static_cast<uint16_t>(data[src]) | (static_cast<uint16_t>(data[src + 1]) << 8));
                    const size_t dst = (y * TH_Picture::imageWidth + x) * 3;
                    rgb[dst] = color[0];
                    rgb[dst + 1] = color[1];
                    rgb[dst + 2] = color[2];
                }
            }

            return rgb;
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
                j["previewImageDataUrl"] = make_bmp_data_url(monoPictureWidth, monoPictureHeight, make_mono_picture_preview_rgb(data));
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
                j["previewImageDataUrl"] = make_bmp_data_url(colorPictureWidth, colorPictureHeight, make_color_picture_preview_rgb(data));
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
                j["previewImageDataUrl"] = make_bmp_data_url(imageWidth, imageHeight, make_image_preview_rgb(data));
                break;

            default:
                throw std::invalid_argument("Unsupported picture/image data size: " + std::to_string(actualSize));
        }

        return j.dump(4);
    }

    TIVarFileMinVersionByte TH_Picture::getMinVersionFromData(const data_t& data)
    {
        if (data.size() == colorPictureDataByteCount)
        {
            return VER_84CSE_ALL;
        }
        return VER_NONE;
    }
}
