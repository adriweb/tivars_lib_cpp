/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 *
 * This code has been mostly possible thanks to LogicalJoe's research on
 * https://github.com/TI-Toolkit/tivars_hexfiend_templates/
 * and kg583's on https://github.com/TI-Toolkit/tivars_lib_py
 */

#include "TIFlashFile.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <ctime>

#include "TIModels.h"
#include "json.hpp"
#include "tivarslib_utils.h"

using json = nlohmann::json;

namespace tivars
{
    const constexpr char TIFlashFile::magicStr[];

    namespace
    {
        uint16_t read_be16(const data_t& data, size_t offset)
        {
            return static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
        }

        uint32_t read_be32(const data_t& data, size_t offset)
        {
            return (static_cast<uint32_t>(data[offset]) << 24)
                 | (static_cast<uint32_t>(data[offset + 1]) << 16)
                 | (static_cast<uint32_t>(data[offset + 2]) << 8)
                 | static_cast<uint32_t>(data[offset + 3]);
        }

        uint32_t read_le32(const data_t& data, size_t offset)
        {
            return static_cast<uint32_t>(data[offset])
                 | (static_cast<uint32_t>(data[offset + 1]) << 8)
                 | (static_cast<uint32_t>(data[offset + 2]) << 16)
                 | (static_cast<uint32_t>(data[offset + 3]) << 24);
        }

        uint32_t read_le24(const data_t& data, size_t offset)
        {
            return static_cast<uint32_t>(data[offset])
                 | (static_cast<uint32_t>(data[offset + 1]) << 8)
                 | (static_cast<uint32_t>(data[offset + 2]) << 16);
        }

        std::string to_hex_string(const data_t& data, size_t offset = 0, size_t length = std::string::npos)
        {
            const size_t end = std::min(data.size(), offset + (length == std::string::npos ? data.size() : length));
            std::string out;
            for (size_t i = offset; i < end; i++)
            {
                out += dechex(data[i]);
            }
            return out;
        }

        std::string flash_field_name(uint16_t value)
        {
            switch (value)
            {
                case 0x000: return "Padding";
                case 0x010: return "Certificate revision";
                case 0x020: return "Date signature";
                case 0x022: return "CSE signature";
                case 0x023: return "CE signature";
                case 0x032: return "Date stamp";
                case 0x033: return "Certificate parent";
                case 0x034: return "Exam LED available";
                case 0x037: return "Minimum installable OS";
                case 0x040: return "Calculator ID";
                case 0x041: return "Validation ID";
                case 0x042: return "Model name";
                case 0x043: return "Python co-processor";
                case 0x051: return "About text";
                case 0x071: return "Standard key header";
                case 0x073: return "Standard key signature";
                case 0x081: return "Custom app key header";
                case 0x083: return "Custom app key data";
                case 0x090: return "Date stamp subfield";
                case 0x0A0: return "Calculator ID-related";
                case 0x0A1: return "Calculator ID";
                case 0x0A2: return "Validation key";
                case 0x0B0: return "Default language";
                case 0x0C0: return "Exam mode status";
                case 0x800: return "Master";
                case 0x801: return "Signing key";
                case 0x802: return "Revision";
                case 0x803: return "Build";
                case 0x804: return "Name";
                case 0x805: return "Expiration date";
                case 0x806: return "Overuse count";
                case 0x807: return "Final";
                case 0x808: return "Page count";
                case 0x809: return "Disable TI splash";
                case 0x80A: return "Max hardware revision";
                case 0x80C: return "Lowest basecode";
                case 0x810: return "Master";
                case 0x811: return "Signing key";
                case 0x812: return "Version";
                case 0x813: return "Build";
                case 0x814: return "Name";
                case 0x817: return "Final";
                case 0x81A: return "Max hardware";
                default:
                {
                    std::ostringstream out;
                    out << std::uppercase << std::hex << std::setw(3) << std::setfill('0') << value;
                    return out.str();
                }
            }
        }

        std::string flash_field_hex(uint16_t value)
        {
            std::ostringstream out;
            out << std::uppercase << std::hex << std::setw(3) << std::setfill('0') << value;
            return out.str();
        }

        std::string format_unix_timestamp(uint32_t timestamp)
        {
            const std::time_t raw = timestamp;
            std::tm tm{};
#ifdef _WIN32
            gmtime_s(&tm, &raw);
#else
            gmtime_r(&raw, &tm);
#endif
            char buffer[32] = {};
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &tm);
            return buffer;
        }

        json parse_flash_fields(const data_t& data, size_t start, size_t end);

        json parse_extended_flash_field(const data_t& data, size_t start, size_t size)
        {
            json out = {
                {"rawDataHex", to_hex_string(data, start, size)}
            };

            if (size >= 3 && std::string(data.begin() + static_cast<ptrdiff_t>(start), data.begin() + static_cast<ptrdiff_t>(start + 3)) == "eZ8")
            {
                out["format"] = "eZ80";
                if (size >= 39)
                {
                    const size_t infoStart = start;
                    const uint32_t mainOffset = read_le24(data, infoStart + 18);
                    out["name"] = std::string(data.begin() + static_cast<ptrdiff_t>(infoStart + 3), data.begin() + static_cast<ptrdiff_t>(infoStart + 12));
                    if (const size_t nulPos = out["name"].get<std::string>().find('\0'); nulPos != std::string::npos)
                    {
                        out["name"] = out["name"].get<std::string>().substr(0, nulPos);
                    }
                    out["flag1"] = data[infoStart + 12];
                    out["unknownFlag"] = data[infoStart + 13];
                    out["flag2"] = data[infoStart + 14];
                    out["mainOffset"] = mainOffset;
                    out["initializedLocation"] = read_le24(data, infoStart + 21);
                    out["initializedSize"] = read_le24(data, infoStart + 24);
                    out["entryPoint"] = read_le24(data, infoStart + 27);
                    out["language"] = read_le24(data, infoStart + 30);
                    out["execLib"] = read_le24(data, infoStart + 33);
                    const uint32_t copyrightOffset = read_le24(data, infoStart + 36);
                    if (copyrightOffset < size)
                    {
                        out["copyright"] = std::string(data.begin() + static_cast<ptrdiff_t>(start + copyrightOffset), data.begin() + static_cast<ptrdiff_t>(start + size));
                        if (const size_t nulPos = out["copyright"].get<std::string>().find('\0'); nulPos != std::string::npos)
                        {
                            out["copyright"] = out["copyright"].get<std::string>().substr(0, nulPos);
                        }
                    }

                    json relocations = json::array();
                    const size_t relocationStart = infoStart + 39;
                    const size_t relocationEnd = std::min(start + size, start + static_cast<size_t>(mainOffset));
                    for (size_t pos = relocationStart; pos + 5 < relocationEnd; pos += 6)
                    {
                        const uint32_t hole = read_le24(data, pos);
                        const uint32_t address = read_le24(data, pos + 3);
                        relocations.push_back({
                            {"hole", hole},
                            {"base", (address >> 22) != 0 ? "Data Base" : "Code Base"},
                            {"offset", address & 0x3FFFFF}
                        });
                    }
                    out["relocationTable"] = relocations;
                    if (mainOffset <= size)
                    {
                        out["bodyHex"] = to_hex_string(data, start + mainOffset, size - mainOffset);
                    }
                }
            }
            else
            {
                out["format"] = "raw";
            }

            return out;
        }

        size_t flash_field_payload_size(const data_t& data, size_t& pos, uint8_t sizeCode)
        {
            switch (sizeCode)
            {
                case 0x0D:
                    return data.at(pos++);
                case 0x0E:
                {
                    const size_t size = read_be16(data, pos);
                    pos += 2;
                    return size;
                }
                case 0x0F:
                {
                    const size_t size = read_be32(data, pos);
                    pos += 4;
                    return size;
                }
                default:
                    return sizeCode;
            }
        }

        std::pair<json, size_t> parse_flash_field(const data_t& data, size_t pos, size_t end)
        {
            if (pos + 1 >= end)
            {
                throw std::runtime_error("Unexpected end of flash field data");
            }

            const size_t fieldStart = pos;
            const uint16_t field = read_be16(data, pos);
            pos += 2;
            const uint16_t fieldId = static_cast<uint16_t>(field >> 4);
            const uint8_t sizeCode = static_cast<uint8_t>(field & 0x0F);
            const size_t payloadSize = flash_field_payload_size(data, pos, sizeCode);
            if (pos + payloadSize > end)
            {
                throw std::runtime_error("Flash field extends beyond available data");
            }

            json j = {
                {"id", fieldId},
                {"idHex", flash_field_hex(fieldId)},
                {"name", flash_field_name(fieldId)},
                {"payloadSize", payloadSize},
                {"headerSize", pos - fieldStart},
                {"offset", fieldStart}
            };

            if ((fieldId == 0x800 || fieldId == 0x810 || fieldId == 0x032) && payloadSize > 0)
            {
                j["fields"] = parse_flash_fields(data, pos, pos + payloadSize);
            }
            else if (fieldId == 0x817 && payloadSize > 0)
            {
                j["extended"] = parse_extended_flash_field(data, pos, payloadSize);
            }
            else if (fieldId == 0x090 && payloadSize == 4)
            {
                const uint32_t timestamp = read_be32(data, pos);
                j["timestamp"] = timestamp;
                j["timestampUtc"] = format_unix_timestamp(timestamp);
            }
            else if (payloadSize > 0)
            {
                j["rawDataHex"] = to_hex_string(data, pos, payloadSize);
            }

            return {j, pos + payloadSize - fieldStart};
        }

        json parse_flash_fields(const data_t& data, size_t start, size_t end)
        {
            json fields = json::array();
            size_t pos = start;
            while (pos < end)
            {
                const auto [field, consumed] = parse_flash_field(data, pos, end);
                fields.push_back(field);
                pos += consumed;
            }
            return fields;
        }
    }

    TIFlashFile::TIFlashFile(const std::string& filePath) : filePath(filePath), fromFile(true)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Can't open the input file");
        }

        const data_t bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bytes.size() < headerByteCountWithoutChecksum)
        {
            throw std::runtime_error("This file is not a valid TI flash file");
        }

        size_t offset = 0;
        while (offset < bytes.size())
        {
            size_t nextOffset = offset;
            headers.push_back(parseHeader(bytes, offset, &nextOffset));
            if (nextOffset <= offset)
            {
                throw std::runtime_error("Internal error while parsing flash headers");
            }
            offset = nextOffset;
        }

        if (headers.empty())
        {
            throw std::runtime_error("No flash headers found in file");
        }
    }

    TIFlashFile::TIFlashFile(const TIVarType& type, const std::string& name, const TIModel& model, bool rawBinaryData)
    {
        addHeader(type, name, model, rawBinaryData);
    }

    TIFlashFile TIFlashFile::loadFromFile(const std::string& filePath)
    {
        if (filePath.empty())
        {
            throw std::invalid_argument("Empty file path given");
        }
        return TIFlashFile(filePath);
    }

    TIFlashFile TIFlashFile::createNew(const TIVarType& type, const std::string& name, const TIModel& model, bool rawBinaryData)
    {
        return TIFlashFile(type, name, model, rawBinaryData);
    }

    TIFlashFile TIFlashFile::createNew(const TIVarType& type, const std::string& name, bool rawBinaryData)
    {
        return createNew(type, name, TIModel{"84+CE"}, rawBinaryData);
    }

    TIFlashFile TIFlashFile::createNew(const TIVarType& type, bool rawBinaryData)
    {
        return createNew(type, "", rawBinaryData);
    }

    void TIFlashFile::addHeader(const TIVarType& type, const std::string& name, const TIModel& model, bool rawBinaryData)
    {
        if (!model.supportsType(type))
        {
            throw std::runtime_error("This calculator model (" + model.getName() + ") does not support the type " + type.getName());
        }

        flash_header_t header{};
        header.type = type;
        header.model = model;
        header.name = name.empty() ? "UNNAMED" : name;
        header.binaryFlag = rawBinaryData ? rawBinaryDataFlag : intelDataFlag;
        header.objectType = defaultObjectTypeFor(type);
        header.productId = static_cast<uint8_t>(model.getProductId());
        header.devices = {{defaultDeviceType, static_cast<uint8_t>(type.getId())}};
        header.calcData = rawBinaryData ? data_t{} : parseHex("00000001FF");
        headers.push_back(header);
    }

    std::string TIFlashFile::getReadableContent(size_t headerIdx) const
    {
        if (headerIdx >= headers.size())
        {
            throw std::out_of_range("Invalid flash header index");
        }

        const flash_header_t& header = headers[headerIdx];
        json j;
        j["typeName"] = header.type.getName();
        j["typeId"] = header.devices.empty() ? -1 : header.devices.front().second;
        j["magic"] = header.magic;
        j["revision"] = header.revision;
        j["binaryFlag"] = header.binaryFlag;
        j["objectType"] = header.objectType;
        j["date"] = {header.date.day, header.date.month, header.date.year};
        j["name"] = header.name;
        j["productId"] = header.productId;
        j["calcDataSize"] = header.calcData.size();
        j["hasChecksum"] = header.hasChecksum;

        j["devices"] = json::array();
        for (const auto& [devType, typeId] : header.devices)
        {
            j["devices"].push_back({
                {"deviceType", devType},
                {"typeId", typeId},
            });
        }

        if (header.binaryFlag == rawBinaryDataFlag)
        {
            j["calcDataHex"] = toHex(header.calcData);
            try
            {
                j["fields"] = parse_flash_fields(header.calcData, 0, header.calcData.size());
            }
            catch (const std::exception& e)
            {
                j["fieldsError"] = e.what();
            }
            if (header.type.getName() == "FlashLicense")
            {
                j["license"] = std::string(header.calcData.begin(), header.calcData.end());
            }
        }
        else
        {
            j["blocks"] = json::array();
            for (const auto& [address, blockType, data] : parseIntelBlocks(header.calcData))
            {
                j["blocks"].push_back({
                    {"address", toHex({static_cast<uint8_t>((address >> 8) & 0xFF), static_cast<uint8_t>(address & 0xFF)})},
                    {"blockType", dechex(blockType)},
                    {"dataHex", toHex(data)},
                });
            }
        }

        return j.dump(4);
    }

    void TIFlashFile::setContentFromString(const std::string& str, size_t headerIdx)
    {
        if (headerIdx >= headers.size())
        {
            throw std::out_of_range("Invalid flash header index");
        }

        auto& [magic, revision, binaryFlag, objectType, date, name, devices, productId, calcData, hasChecksum, type, model] = headers[headerIdx];
        const json j = json::parse(str);
        if (!j.is_object())
        {
            throw std::invalid_argument("Flash header JSON must be an object");
        }

        if (j.contains("typeName"))
        {
            type = TIVarType{j.at("typeName").get<std::string>()};
        }
        if (j.contains("magic"))
        {
            magic = j.at("magic").get<std::string>();
        }
        if (j.contains("revision"))
        {
            revision = j.at("revision").get<std::string>();
        }
        if (j.contains("binaryFlag"))
        {
            binaryFlag = static_cast<uint8_t>(j.at("binaryFlag").get<int>());
        }
        if (j.contains("objectType"))
        {
            objectType = static_cast<uint8_t>(j.at("objectType").get<int>());
        }
        if (j.contains("date"))
        {
            const json& jsonDate = j.at("date");
            if (!jsonDate.is_array() || jsonDate.size() != 3)
            {
                throw std::invalid_argument("Flash header date must be a [day, month, year] array");
            }
            date.day = static_cast<uint8_t>(jsonDate[0].get<int>());
            date.month = static_cast<uint8_t>(jsonDate[1].get<int>());
            date.year = static_cast<uint16_t>(jsonDate[2].get<int>());
        }
        if (j.contains("name"))
        {
            name = j.at("name").get<std::string>();
        }
        if (j.contains("devices"))
        {
            devices.clear();
            for (const json& device : j.at("devices"))
            {
                devices.emplace_back(
                    static_cast<uint8_t>(device.at("deviceType").get<int>()),
                    static_cast<uint8_t>(device.at("typeId").get<int>())
                );
            }
        }
        if (j.contains("productId"))
        {
            productId = static_cast<uint8_t>(j.at("productId").get<int>());
            model = modelFromProductId(productId);
        }
        if (j.contains("hasChecksum"))
        {
            hasChecksum = j.at("hasChecksum").get<bool>();
        }

        if (j.contains("blocks"))
        {
            std::vector<flash_block_t> blocks;
            for (const json& item : j.at("blocks"))
            {
                const data_t addressBytes = parseHex(item.at("address").get<std::string>());
                if (addressBytes.size() != 2)
                {
                    throw std::invalid_argument("Flash block address must be a two-byte hex string");
                }
                blocks.emplace_back(
                    static_cast<uint16_t>((addressBytes[0] << 8) | addressBytes[1]),
                    hexdec(item.at("blockType").get<std::string>()),
                    parseHex(item.at("dataHex").get<std::string>())
                );
            }
            calcData = makeIntelData(blocks);
        }
        else if (j.contains("calcDataHex"))
        {
            calcData = parseHex(j.at("calcDataHex").get<std::string>());
        }
        else if (type.getName() == "FlashLicense" && j.contains("license"))
        {
            const std::string license = j.at("license").get<std::string>();
            calcData.assign(license.begin(), license.end());
        }

        if (!devices.empty())
        {
            type = TIVarType{devices.front().second};
        }
    }

    data_t TIFlashFile::makeBinData() const
    {
        data_t bytes;
        for (const auto& header : headers)
        {
            vector_append(bytes, makeHeaderBytes(header));
        }
        return bytes;
    }

    std::string TIFlashFile::saveToFile(const std::string& path) const
    {
        if (path.empty())
        {
            throw std::invalid_argument("Empty file path given");
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            throw std::runtime_error("Can't open the output file");
        }

        const data_t bytes = makeBinData();
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file)
        {
            throw std::runtime_error("Error while writing flash file");
        }
        return path;
    }

    TIFlashFile::flash_header_t TIFlashFile::parseHeader(const data_t& bytes, size_t offset, size_t* nextOffset)
    {
        const size_t headerLength = nextHeaderLength(bytes, offset);
        if (nextOffset)
        {
            *nextOffset = offset + headerLength;
        }

        size_t pos = offset;
        flash_header_t header{};
        header.magic.assign(bytes.begin() + pos, bytes.begin() + pos + magicByteCount);
        pos += magicByteCount;

        header.revision = parseBCDRevision(data_t(bytes.begin() + pos, bytes.begin() + pos + revisionByteCount));
        pos += revisionByteCount;

        header.binaryFlag = bytes[pos++];
        header.objectType = bytes[pos++];
        header.date = parseBCDDate(data_t(bytes.begin() + pos, bytes.begin() + pos + dateByteCount));
        pos += dateByteCount;

        const uint8_t nameLength = bytes[pos++];
        header.name.assign(bytes.begin() + pos, bytes.begin() + pos + nameByteCount);
        const size_t nulPos = header.name.find('\0');
        if (nulPos != std::string::npos)
        {
            header.name.erase(nulPos);
        }
        if (nameLength < header.name.size())
        {
            header.name.resize(nameLength);
        }
        pos += nameByteCount;

        data_t devicesRaw(bytes.begin() + pos, bytes.begin() + pos + deviceFieldByteCount);
        while (!devicesRaw.empty() && devicesRaw.back() == 0x00)
        {
            devicesRaw.pop_back();
        }
        if (devicesRaw.size() % 2 != 0)
        {
            throw std::runtime_error("Invalid flash device field length");
        }
        for (size_t i = 0; i < devicesRaw.size(); i += 2)
        {
            header.devices.emplace_back(devicesRaw[i], devicesRaw[i + 1]);
        }
        pos += deviceFieldByteCount;

        header.productId = bytes[pos++];
        const uint32_t dataSize = read_le32(bytes, pos);
        pos += dataSizeByteCount;

        if (pos + dataSize > offset + headerLength)
        {
            throw std::runtime_error("Invalid flash header data length");
        }
        header.calcData.assign(bytes.begin() + pos, bytes.begin() + pos + dataSize);
        pos += dataSize;

        header.hasChecksum = (headerLength == headerByteCountWithoutChecksum + dataSize + checksumByteCount);
        if (header.hasChecksum)
        {
            const uint16_t fileChecksum = static_cast<uint16_t>(bytes[pos] | (bytes[pos + 1] << 8));
            const uint16_t computedChecksum = computeChecksum(header.calcData);
            if (fileChecksum != computedChecksum)
            {
                throw std::runtime_error("Invalid flash header checksum");
            }
        }

        if (!header.devices.empty())
        {
            header.type = TIVarType{header.devices.front().second};
        }
        header.model = modelFromProductId(header.productId);
        return header;
    }

    size_t TIFlashFile::nextHeaderLength(const data_t& bytes, size_t offset)
    {
        if (offset + headerByteCountWithoutChecksum > bytes.size())
        {
            throw std::runtime_error("Unexpected end of flash file");
        }

        const uint32_t dataSize = read_le32(bytes, offset + headerByteCountWithoutChecksum - dataSizeByteCount);
        const size_t headerLengthWithoutChecksum = headerByteCountWithoutChecksum + dataSize;
        if (offset + headerLengthWithoutChecksum > bytes.size())
        {
            throw std::runtime_error("Unexpected end of flash file");
        }

        if (offset + headerLengthWithoutChecksum == bytes.size())
        {
            return headerLengthWithoutChecksum;
        }

        if (offset + headerLengthWithoutChecksum + magicByteCount <= bytes.size()
         && std::equal(magicStr, magicStr + magicByteCount, bytes.begin() + static_cast<long>(offset + headerLengthWithoutChecksum)))
        {
            return headerLengthWithoutChecksum;
        }

        return headerLengthWithoutChecksum + checksumByteCount;
    }

    data_t TIFlashFile::makeHeaderBytes(const flash_header_t& header)
    {
        data_t bytes;
        const std::string nullPad(1, '\0');
        const std::string magic = str_pad(header.magic, magicByteCount, nullPad).substr(0, magicByteCount);
        bytes.insert(bytes.end(), magic.begin(), magic.end());

        const data_t revisionBytes = makeBCDRevision(header.revision);
        vector_append(bytes, revisionBytes);

        bytes.push_back(header.binaryFlag);
        bytes.push_back(header.objectType);

        vector_append(bytes, makeBCDDate(header.date));

        const std::string trimmedName = header.name.substr(0, nameByteCount);
        bytes.push_back(static_cast<uint8_t>(trimmedName.size()));
        const std::string paddedName = str_pad(trimmedName, nameByteCount, nullPad);
        bytes.insert(bytes.end(), paddedName.begin(), paddedName.end());

        data_t devicesField;
        for (const auto& device : header.devices)
        {
            devicesField.push_back(device.first);
            devicesField.push_back(device.second);
        }
        devicesField.resize(deviceFieldByteCount, 0x00);
        vector_append(bytes, devicesField);

        bytes.push_back(header.productId);

        const uint32_t dataSize = static_cast<uint32_t>(header.calcData.size());
        bytes.push_back(static_cast<uint8_t>(dataSize & 0xFF));
        bytes.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));

        vector_append(bytes, header.calcData);

        if (header.hasChecksum)
        {
            const uint16_t checksum = computeChecksum(header.calcData);
            bytes.push_back(static_cast<uint8_t>(checksum & 0xFF));
            bytes.push_back(static_cast<uint8_t>((checksum >> 8) & 0xFF));
        }

        return bytes;
    }

    uint16_t TIFlashFile::computeChecksum(const data_t& data)
    {
        uint32_t sum = 0;
        for (const uint8_t byte : data)
        {
            sum += byte;
        }
        return static_cast<uint16_t>(sum & 0xFFFF);
    }

    std::vector<TIFlashFile::flash_block_t> TIFlashFile::parseIntelBlocks(const data_t& data)
    {
        std::vector<flash_block_t> blocks;
        size_t start = 0;
        while (start < data.size())
        {
            size_t end = start;
            while (end < data.size() && !(data[end] == '\r' && end + 1 < data.size() && data[end + 1] == '\n'))
            {
                end++;
            }

            if (end > start)
            {
                const std::string record(data.begin() + static_cast<long>(start), data.begin() + static_cast<long>(end));
                blocks.push_back(recordToBlock(record));
            }

            start = (end + 1 < data.size()) ? end + 2 : data.size();
        }
        return blocks;
    }

    data_t TIFlashFile::makeIntelData(const std::vector<flash_block_t>& blocks)
    {
        data_t data;
        for (size_t i = 0; i < blocks.size(); i++)
        {
            const std::string record = blockToRecord(blocks[i]);
            data.insert(data.end(), record.begin(), record.end());
            if (i + 1 < blocks.size())
            {
                data.push_back('\r');
                data.push_back('\n');
            }
        }
        return data;
    }

    std::string TIFlashFile::blockToRecord(const flash_block_t& block)
    {
        std::ostringstream out;
        out << ':';
        out << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << static_cast<int>(block.data.size())
            << std::setw(4) << static_cast<int>(block.address)
            << std::setw(2) << static_cast<int>(block.blockType);
        for (const uint8_t byte : block.data)
        {
            out << std::setw(2) << static_cast<int>(byte);
        }

        uint32_t checksumBase = static_cast<uint32_t>(block.data.size())
                              + ((block.address >> 8) & 0xFF)
                              + (block.address & 0xFF)
                              + block.blockType;
        for (const uint8_t byte : block.data)
        {
            checksumBase += byte;
        }
        out << std::setw(2) << (-static_cast<int>(checksumBase) & 0xFF);
        return out.str();
    }

    TIFlashFile::flash_block_t TIFlashFile::recordToBlock(const std::string& record)
    {
        if (record.size() < 11 || record.front() != ':')
        {
            throw std::invalid_argument("Invalid Intel flash record");
        }

        const int size = hexdec(record.substr(1, 2));
        const int expectedHexLen = 1 + 2 + 4 + 2 + size * 2 + 2;
        if (static_cast<int>(record.size()) != expectedHexLen)
        {
            throw std::invalid_argument("Unexpected Intel flash record length");
        }

        flash_block_t block{};
        block.address = static_cast<uint16_t>(std::stoul(record.substr(3, 4), nullptr, 16));
        block.blockType = hexdec(record.substr(7, 2));
        block.data = parseHex(record.substr(9, size * 2));

        const uint8_t fileChecksum = hexdec(record.substr(record.size() - 2, 2));
        uint32_t checksumBase = static_cast<uint32_t>(block.data.size())
                              + ((block.address >> 8) & 0xFF)
                              + (block.address & 0xFF)
                              + block.blockType;
        for (const uint8_t byte : block.data)
        {
            checksumBase += byte;
        }
        const uint8_t computedChecksum = static_cast<uint8_t>((-static_cast<int>(checksumBase)) & 0xFF);
        if (fileChecksum != computedChecksum)
        {
            throw std::invalid_argument("Invalid Intel flash record checksum");
        }

        return block;
    }

    std::string TIFlashFile::extensionFor(const flash_header_t& header)
    {
        const std::vector<std::string>& exts = header.type.getExts();
        const int orderId = header.model.getOrderId();
        if (orderId >= 0 && orderId < static_cast<int>(exts.size()) && !exts[orderId].empty())
        {
            return exts[orderId];
        }
        for (const std::string& ext : exts)
        {
            if (!ext.empty())
            {
                return ext;
            }
        }
        return "";
    }

    uint8_t TIFlashFile::defaultObjectTypeFor(const TIVarType& type)
    {
        return (type.getName() == "FlashApp") ? 0x88 : 0x00;
    }

    data_t TIFlashFile::parseHex(const std::string& hex)
    {
        if (hex.size() % 2 != 0)
        {
            throw std::invalid_argument("Hex strings must have an even number of digits");
        }

        data_t data;
        data.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            data.push_back(hexdec(hex.substr(i, 2)));
        }
        return data;
    }

    std::string TIFlashFile::toHex(const data_t& data)
    {
        std::ostringstream out;
        out << std::uppercase << std::hex << std::setfill('0');
        for (const uint8_t byte : data)
        {
            out << std::setw(2) << static_cast<int>(byte);
        }
        return out.str();
    }

    uint8_t TIFlashFile::encodeBCDByte(uint8_t value)
    {
        if (value > 99)
        {
            throw std::invalid_argument("BCD byte out of range");
        }
        return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
    }

    uint8_t TIFlashFile::decodeBCDByte(uint8_t value)
    {
        return static_cast<uint8_t>(((value >> 4) & 0x0F) * 10 + (value & 0x0F));
    }

    TIFlashFile::flash_date_t TIFlashFile::parseBCDDate(const data_t& dateBytes)
    {
        if (dateBytes.size() != dateByteCount)
        {
            throw std::invalid_argument("Invalid flash BCD date length");
        }

        flash_date_t date{};
        date.day = decodeBCDByte(dateBytes[0]);
        date.month = decodeBCDByte(dateBytes[1]);
        date.year = static_cast<uint16_t>(decodeBCDByte(dateBytes[2]) * 100 + decodeBCDByte(dateBytes[3]));
        return date;
    }

    data_t TIFlashFile::makeBCDDate(const flash_date_t& date)
    {
        if (date.year > 9999)
        {
            throw std::invalid_argument("Flash header year out of range");
        }

        return {
            encodeBCDByte(date.day),
            encodeBCDByte(date.month),
            encodeBCDByte(static_cast<uint8_t>(date.year / 100)),
            encodeBCDByte(static_cast<uint8_t>(date.year % 100)),
        };
    }

    std::string TIFlashFile::parseBCDRevision(const data_t& revisionBytes)
    {
        if (revisionBytes.size() != revisionByteCount)
        {
            throw std::invalid_argument("Invalid flash BCD revision length");
        }

        return std::to_string(decodeBCDByte(revisionBytes[0])) + "." + std::to_string(decodeBCDByte(revisionBytes[1]));
    }

    data_t TIFlashFile::makeBCDRevision(const std::string& revision)
    {
        const size_t dotPos = revision.find('.');
        if (dotPos == std::string::npos)
        {
            throw std::invalid_argument("Flash header revision must be in x.y format");
        }

        const int major = std::stoi(revision.substr(0, dotPos));
        const int minor = std::stoi(revision.substr(dotPos + 1));
        if (major < 0 || major > 99 || minor < 0 || minor > 99)
        {
            throw std::invalid_argument("Flash header revision components must fit in BCD");
        }

        return {
            encodeBCDByte(static_cast<uint8_t>(major)),
            encodeBCDByte(static_cast<uint8_t>(minor)),
        };
    }

    TIModel TIFlashFile::modelFromProductId(uint8_t productId)
    {
        if (TIModels::isValidPID(productId))
        {
            return TIModels::fromPID(productId);
        }
        return TIModel{"84+CE"};
    }
}
