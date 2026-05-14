/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarFile.h"
#include "EvoFormat.h"

#include <iomanip>

#include "tivarslib_utils.h"
#include "TIModels.h"

#include <stdexcept>
#include <numeric>
#include <sstream>
#include <cstring>
#include <cctype>
#include <ranges>
#include <unordered_set>

#include "TIVarTypes.h"
#include "json.hpp"

namespace tivars
{
    using namespace EvoFormat;
    namespace
    {
        constexpr uint8_t backupTypeId = 0x13;

        bool is_backup_entry_layout(uint8_t typeId, uint16_t metaLength)
        {
            return typeId == backupTypeId
                && (metaLength == TypeHandlers::TH_Backup::onFileMetaLength3
                 || metaLength == TypeHandlers::TH_Backup::onFileMetaLength4);
        }

        bool is_backup_entry(const TIVarFile::var_entry_t& entry)
        {
            return entry.typeID == backupTypeId && entry._type.getName() == "Backup";
        }

        int count_bits(uint32_t value)
        {
            int count = 0;
            while (value != 0)
            {
                count += static_cast<int>(value & 1U);
                value >>= 1U;
            }
            return count;
        }

        uint32_t infer_required_model_flags(const std::vector<TIVarFile::var_entry_t>& entries)
        {
            uint32_t requiredFlags = 0;
            for (const auto& entry : entries)
            {
                const std::string& typeName = entry._type.getName();
                const TIVarFileMinVersionByte maskedVersion = (TIVarFileMinVersionByte)(entry.version & ~MASK_USES_RTC);
                if (maskedVersion == VER_CE_EXACTONLY || typeName.rfind("Exact", 0) == 0)
                {
                    requiredFlags |= hasExactMath;
                }
                if (maskedVersion == VER_CE_PYTHONMOD || typeName.rfind("Python", 0) == 0)
                {
                    requiredFlags |= hasPython;
                }
            }
            return requiredFlags;
        }

        bool model_supports_all_entries(const TIModel& model, const std::vector<TIVarFile::var_entry_t>& entries)
        {
            return std::ranges::all_of(entries, [&model](const auto& entry) { return model.supportsType(entry._type); });
        }

        TIModel inferLoadedModel(const TIModel& currentModel, const std::vector<TIVarFile::var_entry_t>& entries)
        {
            if (entries.empty())
            {
                return currentModel;
            }

            const uint32_t specialFlagsMask = hasExactMath | hasPython;
            const uint32_t requiredFlags = infer_required_model_flags(entries);
            const auto is_candidate = [&](const TIModel& model, bool enforceProductId)
            {
                return model.getSig() == currentModel.getSig()
                    && (!enforceProductId || currentModel.getProductId() == 0 || model.getProductId() == currentModel.getProductId())
                    && (model.getFlags() & requiredFlags) == requiredFlags
                    && model_supports_all_entries(model, entries);
            };

            TIModel bestModel = currentModel;
            const bool currentModelIsCandidate = is_candidate(currentModel, true);
            bool found = currentModelIsCandidate;
            int bestExtraFlags = found ? count_bits(currentModel.getFlags() & specialFlagsMask & ~requiredFlags) : 0;
            int bestOrderId = found ? currentModel.getOrderId() : 0;
            const bool enforceProductId = currentModelIsCandidate;

            std::unordered_set<std::string> seenNames;
            for (const auto& model : TIModels::all() | std::views::values)
            {
                if (!seenNames.insert(model.getName()).second || !is_candidate(model, enforceProductId))
                {
                    continue;
                }

                const int extraFlags = count_bits(model.getFlags() & specialFlagsMask & ~requiredFlags);
                if (!found
                 || extraFlags < bestExtraFlags
                 || (extraFlags == bestExtraFlags && model.getOrderId() < bestOrderId))
                {
                    bestModel = model;
                    bestExtraFlags = extraFlags;
                    bestOrderId = model.getOrderId();
                    found = true;
                }
            }

            return found ? bestModel : currentModel;
        }

        std::string uppercase_ascii(std::string s)
        {
            for (char& c : s)
            {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return s;
        }

        std::string normalize_theta_chars(std::string name)
        {
            for (const auto& token : {"θ", "Θ", "ϴ", "ᶿ"})
            {
                replace_all(name, token, "[");
            }
            return name;
        }

        std::string make_indexed_var_name(uint8_t prefix, uint8_t index)
        {
            return std::string{static_cast<char>(prefix), static_cast<char>(index)};
        }

        int parse_ti_digit(char c)
        {
            if (c >= '1' && c <= '9')
            {
                return c - '1';
            }
            if (c == '0')
            {
                return 9;
            }
            return -1;
        }

        bool is_name_char(char c)
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            return std::isdigit(uc) || (uc >= 'A' && uc <= 'Z') || c == '[';
        }

        bool is_alpha_or_theta(char c)
        {
            return (c >= 'A' && c <= 'Z') || c == '[';
        }

        std::string default_var_name_for_type(const TIVarType& type)
        {
            const auto& typeName = type.getName();
            const auto typeId = type.getId();
            if (typeName == "Real" || typeName == "Complex" || (typeId >= 0x1B && typeId <= 0x21))
            {
                return "A";
            }
            if (typeName == "RealList" || typeName == "ComplexList")
            {
                return make_indexed_var_name(0x5D, 0x00);
            }
            if (typeName == "Matrix")
            {
                return make_indexed_var_name(0x5C, 0x00);
            }
            if (typeName == "Equation" || typeName == "SmartEquation")
            {
                return make_indexed_var_name(0x5E, 0x10);
            }
            if (typeName == "String")
            {
                return make_indexed_var_name(0xAA, 0x00);
            }
            if (typeName == "Picture")
            {
                return make_indexed_var_name(0x60, 0x00);
            }
            if (typeName == "Image")
            {
                return make_indexed_var_name(0x3C, 0x00);
            }
            if (typeName == "GraphDataBase")
            {
                return make_indexed_var_name(0x61, 0x00);
            }
            if (typeName == "TableRange")
            {
                return "TblSet";
            }
            if (typeName == "WindowSettings")
            {
                return "Window";
            }
            if (typeName == "RecallWindow")
            {
                return "RclWindw";
            }

            return "FILE" + (type.getExts().empty() ? "" : type.getExts()[0]);
        }

        bool normalize_list_name(std::string& name)
        {
            if (!name.empty() && static_cast<uint8_t>(name[0]) == 0x5D)
            {
                if (name.size() == 1)
                {
                    name.push_back('\0');
                    return true;
                }
                if ((name.size() == 2 && static_cast<uint8_t>(name[1]) <= 0x05) || (name.size() == 2 && static_cast<uint8_t>(name[1]) == 0x40))
                {
                    return true;
                }
                if (name.size() > 6 || std::isdigit(static_cast<unsigned char>(name[1])))
                {
                    return false;
                }
                for (size_t i = 1; i < name.size(); i++)
                {
                    if (!is_name_char(name[i]))
                    {
                        return false;
                    }
                }
                return true;
            }
            const std::string upperName = uppercase_ascii(name);
            if (upperName == "IDLIST")
            {
                name = make_indexed_var_name(0x5D, 0x40);
                return true;
            }
            if (upperName.size() == 2 && upperName[0] == 'L' && upperName[1] >= '1' && upperName[1] <= '6')
            {
                name = make_indexed_var_name(0x5D, static_cast<uint8_t>(upperName[1] - '1'));
                return true;
            }
            if (upperName.empty() || upperName.size() > 5 || std::isdigit(static_cast<unsigned char>(upperName[0])))
            {
                return false;
            }
            for (const char c : upperName)
            {
                if (!is_name_char(c))
                {
                    return false;
                }
            }
            name = std::string(1, 0x5D) + upperName;
            return true;
        }

        bool normalize_matrix_name(std::string& name)
        {
            if (!name.empty() && static_cast<uint8_t>(name[0]) == 0x5C)
            {
                if (name.size() == 1)
                {
                    name.push_back('\0');
                    return true;
                }
                return name.size() == 2 && static_cast<uint8_t>(name[1]) <= 0x09;
            }
            std::string upperName = uppercase_ascii(name);
            if (upperName.size() == 3 && upperName.front() == '[' && upperName.back() == ']')
            {
                upperName = upperName.substr(1, 1);
            }
            if (upperName.size() != 1 || upperName[0] < 'A' || upperName[0] > 'J')
            {
                return false;
            }
            name = make_indexed_var_name(0x5C, static_cast<uint8_t>(upperName[0] - 'A'));
            return true;
        }

        bool normalize_equation_name(std::string& name)
        {
            if (!name.empty() && static_cast<uint8_t>(name[0]) == 0x5E)
            {
                if (name.size() != 2)
                {
                    return false;
                }
                const uint8_t idx = static_cast<uint8_t>(name[1]);
                return (idx >= 0x10 && idx <= 0x2B) || (idx >= 0x40 && idx <= 0x45) || (idx >= 0x80 && idx <= 0x82);
            }
            std::string upperName = uppercase_ascii(name);
            if (upperName == "U" || upperName == "|U")
            {
                name = make_indexed_var_name(0x5E, 0x80);
                return true;
            }
            if (upperName == "V" || upperName == "|V")
            {
                name = make_indexed_var_name(0x5E, 0x81);
                return true;
            }
            if (upperName == "W" || upperName == "|W")
            {
                name = make_indexed_var_name(0x5E, 0x82);
                return true;
            }
            if (upperName.size() >= 2 && upperName.front() == '{' && upperName.back() == '}')
            {
                upperName = upperName.substr(1, upperName.size() - 2);
            }
            if (upperName.size() == 2 && upperName[0] == 'Y')
            {
                const int idx = parse_ti_digit(upperName[1]);
                if (idx >= 0)
                {
                    name = make_indexed_var_name(0x5E, static_cast<uint8_t>(0x10 + idx));
                    return true;
                }
            }
            if (upperName.size() == 3 && (upperName[0] == 'X' || upperName[0] == 'Y') && upperName[2] == 'T' && upperName[1] >= '1' && upperName[1] <= '6')
            {
                const uint8_t base = upperName[0] == 'X' ? 0x20 : 0x21;
                const uint8_t idx = static_cast<uint8_t>((upperName[1] - '1') * 2);
                name = make_indexed_var_name(0x5E, static_cast<uint8_t>(base + idx));
                return true;
            }
            if (upperName.size() == 2 && upperName[0] == 'R' && upperName[1] >= '1' && upperName[1] <= '6')
            {
                name = make_indexed_var_name(0x5E, static_cast<uint8_t>(0x40 + (upperName[1] - '1')));
                return true;
            }
            return false;
        }

        bool normalize_decimal_slot_name(std::string& name, const std::string& prefix, uint8_t leadingByte)
        {
            if (!name.empty() && static_cast<uint8_t>(name[0]) == leadingByte)
            {
                if (name.size() == 1)
                {
                    name.push_back('\0');
                    return true;
                }
                return name.size() == 2 && static_cast<uint8_t>(name[1]) <= 0x09;
            }
            const std::string upperName = uppercase_ascii(name);
            if (upperName.rfind(prefix, 0) != 0 || upperName.size() != prefix.size() + 1)
            {
                return false;
            }
            const int idx = parse_ti_digit(upperName.back());
            if (idx < 0)
            {
                return false;
            }
            name = make_indexed_var_name(leadingByte, static_cast<uint8_t>(idx));
            return true;
        }

        bool normalize_fixed_ascii_name(std::string& name, const std::string& expected)
        {
            if (name == expected)
            {
                return true;
            }
            if (uppercase_ascii(name) == uppercase_ascii(expected))
            {
                name = expected;
                return true;
            }
            return false;
        }
    }

    /*** Constructors ***/

    /**
     * Internal constructor, called from loadFromFile and createNew.
     * If filePath empty or not given, it's a programmer error, and it will throw in BinaryFile(filePath) anyway.
     * To create a TIVarFile not from a file, use TIVarFile::createNew
     * @param   filePath
     * @throws  \Exception
     */
    TIVarFile::TIVarFile(const std::string& filePath) : BinaryFile(filePath)
    {
        this->fromFile = true;
        if (this->fileSize >= 3)
        {
            rewind(this->file);
            const data_t fileData = this->get_raw_bytes(this->fileSize);
            if (is_evo_file_data(fileData))
            {
                this->evoFormat = true;
                this->makeEvoVarEntryFromFile();
                this->computedChecksum = this->computeEvoChecksumFromFileData();
                this->fileChecksum = this->getChecksumValueFromFile();
                if (this->computedChecksum != this->fileChecksum)
                {
                    printf("[Warning] Evo file is corrupt (file checksum = 0x%04X, calculated checksum = 0x%04X)\n", this->fileChecksum, this->computedChecksum);
                    this->corrupt = true;
                }
                this->close();
                return;
            }
        }
        if (this->fileSize < 76) // bare minimum for header + a var entry
        {
            throw std::runtime_error("This file is not a valid TI-[e]z80 variable file");
        }
        this->makeHeaderFromFile();
        this->makeVarEntriesFromFile();
        this->checkVarEntriesVersionCompat();
        this->computedChecksum = this->computeChecksumFromFileData();
        this->fileChecksum = this->getChecksumValueFromFile();
        if (this->computedChecksum != this->fileChecksum)
        {
            printf("[Warning] File is corrupt (file checksum = 0x%02X, calculated checksum = 0x%02X)\n", this->fileChecksum, this->computedChecksum);
            this->corrupt = true;
        }
        this->close(); // let's free the resource up as early as possible
    }

    TIVarFile TIVarFile::loadFromFile(const std::string& filePath)
    {
        if (!filePath.empty())
        {
            return TIVarFile(filePath);
        } else {
            throw std::runtime_error("No file path given");
        }
    }

    TIVarFile::TIVarFile(const TIVarType& type, const std::string& name, const TIModel& model) : calcModel(model)
    {
        if (!this->calcModel.supportsType(type))
        {
            throw std::runtime_error("This calculator model (" + this->calcModel.getName() + ") does not support the type " + type.getName());
        }

        this->evoFormat = (this->calcModel.getFlags() & TIFeatureFlags::hasEvoASIC) != 0;

        const std::string signature = this->calcModel.getSig();
        const std::string comment = str_pad("Created by tivars_lib_cpp", sizeof(var_header_t::comment), "\0");

        std::ranges::copy(signature, this->header.signature);
        std::ranges::copy(comment, this->header.comment);
        this->header.entries_len = 0; // will have to be overwritten later

        this->entries.resize(1);
        auto& entry = this->entries[0];
        entry.meta_length  = this->evoFormat ? 0
                           : ((type.getName() == "Backup")
                           ? TypeHandlers::TH_Backup::onFileMetaLength3
                           : ((this->calcModel.getFlags() & TIFeatureFlags::hasFlash) ? varEntryNewLength : varEntryOldLength));
        entry.data_length  = 0; // will have to be overwritten later
        entry.typeID       = (uint8_t) type.getId();
        entry._type = type;
        entry.setVarName(name);
        if (this->evoFormat)
        {
            entry.evoTypeID = evo_type_from_type(type, 0);
            entry.evoMetaVersion = 1;
            entry.evoMetaFlags = (entry.evoTypeID == 5 || entry.evoTypeID == 4) ? 1 : 0;
            entry.evoFields["version"] = 1;
        }
        entry.data_length2 = 0; // will have to be overwritten later
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const std::string& name, const TIModel& model)
    {
        return TIVarFile(type, name, model);
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const std::string& name)
    {
        return TIVarFile(type, name, TIModel{"84+CE"});
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type)
    {
        return createNew(type, "");
    }

    /*** Makers ***/

    void TIVarFile::makeHeaderFromFile()
    {
        rewind(this->file);

        const auto signature = this->get_string_bytes(sizeof(var_header_t::signature));
        const auto sig_extra = this->get_raw_bytes(sizeof(var_header_t::sig_extra));
        this->header.ownerPID = this->get_raw_byte();
        const auto comment = this->get_string_bytes(sizeof(var_header_t::comment));
        std::ranges::copy(signature, this->header.signature);
        std::ranges::copy(sig_extra, this->header.sig_extra);
        std::ranges::copy(comment, this->header.comment);
        this->header.entries_len  = this->get_two_bytes_swapped();

        // the calcModel may later get updated with a more precise one, see TIVarFile::interLoadedModel
        if (TIModels::isValidPID(header.ownerPID))
            this->calcModel = TIModels::fromPID(header.ownerPID);
        else if (TIModels::isValidSignature(signature))
            this->calcModel = TIModels::fromSignature(signature);
        else
            throw std::invalid_argument("Unhandled file type. No known model usable for this header.");
    }

    void TIVarFile::makeVarEntriesFromFile()
    {
        fseek(this->file, TIVarFile::firstVarEntryOffset, SEEK_SET);

        while (ftell(this->file) < (long)(this->fileSize - 2))
        {
            var_entry_t entry{};
            entry.meta_length = this->get_two_bytes_swapped();
            entry.data_length = this->get_two_bytes_swapped();
            entry.typeID      = this->get_raw_byte();
            if (is_backup_entry_layout(entry.typeID, entry.meta_length))
            {
                const uint16_t data2Length = this->get_two_bytes_swapped();
                const uint16_t data3Length = this->get_two_bytes_swapped();
                const uint16_t addressOfData2 = this->get_two_bytes_swapped();
                const bool hasData4 = entry.meta_length == TypeHandlers::TH_Backup::onFileMetaLength4;
                const uint16_t data4Length = hasData4 ? this->get_two_bytes_swapped() : 0;
                entry.data_length2 = this->get_two_bytes_swapped();
                const data_t data1 = this->get_raw_bytes(entry.data_length2);
                const data_t data2 = this->get_raw_bytes(data2Length);
                const data_t data3 = this->get_raw_bytes(data3Length);
                const data_t data4 = hasData4 ? this->get_raw_bytes(data4Length) : data_t{};

                entry.data = TypeHandlers::TH_Backup::buildInternal({
                    .hasData4 = hasData4,
                    .addressOfData2 = addressOfData2,
                    .data1 = data1,
                    .data2 = data2,
                    .data3 = data3,
                    .data4 = data4,
                });
                entry.determineFullType();
                this->entries.push_back(entry);
                continue;
            }

            const std::string varNameFromFile = this->get_string_bytes(sizeof(var_entry_t::varname));
            if (entry.meta_length == varEntryNewLength)
            {
                if ((this->calcModel.getFlags() & TIFeatureFlags::hasFlash) == 0)
                {
                    fprintf(stderr, "Something is wrong with your file... The var entry meta length indicates is has flash-related fields, but the signature doesn't match...\n");
                }
                entry.version      = (TIVarFileMinVersionByte)this->get_raw_byte();
                entry.archivedFlag = this->get_raw_byte();
            }
            else if (entry.meta_length != varEntryOldLength)
            {
                throw std::invalid_argument("Invalid file. The var entry meta length has an unexpected value. Don't know what to do with that file.");
            }
            entry.data_length2 = this->get_two_bytes_swapped();
            entry.data         = this->get_raw_bytes(entry.data_length);

            // Now that we have the data, we can retrieve the (full) type...
            entry.determineFullType();

            // We preserve the raw on-file varname bytes on load instead of re-normalizing through setVarName().
            std::ranges::copy(varNameFromFile, entry.varname);

            this->entries.push_back(entry);
        }

        this->calcModel = inferLoadedModel(this->calcModel, this->entries);
    }

    void TIVarFile::makeEvoVarEntryFromFile()
    {
        rewind(this->file);
        data_t fileData = this->get_raw_bytes(this->fileSize);
        if (fileData.size() < 3)
        {
            throw std::invalid_argument("Invalid Evo file. File is too small.");
        }

        const data_t body(fileData.begin(), fileData.end() - 2);
        size_t offset = 0;
        const EvoCBORValue root = parse_cbor_value(body, offset);
        if (root.kind != EvoCBORValue::Kind::Map)
        {
            throw std::invalid_argument("Invalid Evo file. Expected a top-level CBOR map.");
        }

        const auto metaIt = root.map.find("metaData");
        if (metaIt == root.map.end() || metaIt->second.kind != EvoCBORValue::Kind::Map)
        {
            throw std::invalid_argument("Invalid Evo file. Missing metaData map.");
        }

        const auto& meta = metaIt->second.map;
        const auto read_uint_field = [](const std::map<std::string, EvoCBORValue>& map, const std::string& key, uint64_t fallback) -> uint64_t
        {
            const auto it = map.find(key);
            if (it == map.end() || it->second.kind != EvoCBORValue::Kind::Unsigned)
            {
                return fallback;
            }
            return it->second.unsignedValue;
        };

        const auto nameIt = meta.find("name");
        const data_t nameBytes = (nameIt != meta.end() && nameIt->second.kind == EvoCBORValue::Kind::Bytes) ? nameIt->second.bytes : data_t{};
        const uint8_t evoTypeID = static_cast<uint8_t>(read_uint_field(meta, "type", 8));
        const TIVarType mappedType = type_from_evo_type(evoTypeID);

        var_entry_t entry{};
        entry.evoTypeID = evoTypeID;
        entry.evoMetaVersion = static_cast<uint8_t>(read_uint_field(meta, "version", 1));
        entry.evoMetaFlags = static_cast<uint8_t>(read_uint_field(meta, "flags", 0));
        entry.evoNameBytes = nameBytes;
        entry.typeID = static_cast<uint8_t>(mappedType.getId());
        entry._type = mappedType;

        const std::string displayName = decode_evo_name(evoTypeID, nameBytes);
        const std::string paddedName = str_pad(displayName, sizeof(var_entry_t::varname), "\0");
        std::ranges::copy(paddedName.substr(0, sizeof(var_entry_t::varname)), entry.varname);

        for (const auto& [key, value] : root.map)
        {
            if (key == "metaData")
            {
                continue;
            }
            if (key == "data")
            {
                if (value.kind == EvoCBORValue::Kind::Bytes)
                {
                    entry.data = value.bytes;
                    entry.evoDataIsRawCBOR = false;
                }
                else
                {
                    entry.data = value.raw;
                    entry.evoDataIsRawCBOR = true;
                }
            }
            else if (value.kind == EvoCBORValue::Kind::Unsigned)
            {
                entry.evoFields[key] = value.unsignedValue;
            }
        }

        entry.data_length = entry.data_length2 = static_cast<uint16_t>(entry.data.size());
        this->entries.push_back(entry);
        this->calcModel = TIModels::fromName("84Evo");
    }


    /*** Private actions ***/

    uint16_t TIVarFile::computeChecksumFromFileData() const
    {
        if (this->evoFormat)
        {
            return computeEvoChecksumFromFileData();
        }
        if (this->fromFile)
        {
            fseek(this->file, TIVarFile::firstVarEntryOffset, SEEK_SET);

            uint16_t sum = 0;
            for (size_t i = firstVarEntryOffset; i < this->fileSize - 2; i++)
            {
                sum += this->get_raw_byte();
            }
            return (uint16_t) (sum & 0xFFFF);
        } else {
            throw std::runtime_error("[Error] No file loaded");
        }
    }

    uint16_t TIVarFile::computeEvoChecksumFromFileData() const
    {
        if (!this->fromFile)
        {
            throw std::runtime_error("[Error] No file loaded");
        }
        rewind(this->file);
        const data_t fileData = this->get_raw_bytes(this->fileSize);
        if (fileData.size() < 2)
        {
            return 0;
        }
        return evo_checksum(data_t(fileData.begin(), fileData.end() - 2));
    }

    uint16_t TIVarFile::computeChecksumFromInstanceData() const
    {
        if (this->evoFormat)
        {
            return computeEvoChecksumFromInstanceData();
        }
        uint16_t sum = 0;
        for (const auto& entry : this->entries)
        {
            if (is_backup_entry(entry))
            {
                const auto backup = TypeHandlers::TH_Backup::parseInternal(entry.data);
                const auto add_le16_bytes = [&sum](uint16_t value)
                {
                    sum += static_cast<uint8_t>(value & 0xFF);
                    sum += static_cast<uint8_t>((value >> 8) & 0xFF);
                };

                const uint16_t data1Length = static_cast<uint16_t>(backup.data1.size());
                add_le16_bytes(entry.meta_length);
                add_le16_bytes(data1Length);
                sum += entry.typeID;
                add_le16_bytes(static_cast<uint16_t>(backup.data2.size()));
                add_le16_bytes(static_cast<uint16_t>(backup.data3.size()));
                add_le16_bytes(backup.addressOfData2);
                if (backup.hasData4)
                {
                    add_le16_bytes(static_cast<uint16_t>(backup.data4.size()));
                }
                add_le16_bytes(data1Length);
                sum += std::accumulate(backup.data1.begin(), backup.data1.end(), 0);
                sum += std::accumulate(backup.data2.begin(), backup.data2.end(), 0);
                sum += std::accumulate(backup.data3.begin(), backup.data3.end(), 0);
                sum += std::accumulate(backup.data4.begin(), backup.data4.end(), 0);
                continue;
            }

            sum += entry.meta_length;
            sum += entry.typeID;
            sum += 2 * ((entry.data_length & 0xFF) + ((entry.data_length >> 8) & 0xFF)); // 2* because of the two same length fields
            sum += std::accumulate(entry.varname, entry.varname + sizeof(var_entry_t::varname), 0);
            sum += std::accumulate(entry.data.begin(), entry.data.end(), 0);
            if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
            {
                sum += entry.version;
                sum += entry.archivedFlag;
            }
        }
        return (uint16_t) (sum & 0xFFFF);
    }

    uint16_t TIVarFile::computeEvoChecksumFromInstanceData() const
    {
        return evo_checksum(make_evo_bin_data());
    }

    uint16_t TIVarFile::getChecksumValueFromFile()
    {
        if (this->fromFile)
        {
            fseek(this->file, this->fileSize - 2, SEEK_SET);
            if (this->evoFormat)
            {
                const uint8_t high = this->get_raw_byte();
                const uint8_t low = this->get_raw_byte();
                return static_cast<uint16_t>((high << 8) | low);
            }
            return this->get_two_bytes_swapped();
        } else {
            throw std::runtime_error("[Error] No file loaded");
        }
    }

    /**
     *  Updates the length fields in both the header and the var entries, as well as the checksum
     */
    void TIVarFile::refreshMetadataFields()
    {
        if (this->evoFormat)
        {
            for (auto& entry : this->entries)
            {
                entry.data_length = entry.data_length2 = static_cast<uint16_t>(entry.data.size());
                if (!entry.evoDataIsRawCBOR)
                {
                    if ((entry._type.getName() == "RealList" || entry._type.getName() == "ComplexList")
                        && entry.evoFields.contains("len") && entry.evoFields.at("len") == 0 && entry.data.empty())
                    {
                        entry.evoFields.erase("arraylen");
                        entry.evoFields.erase("size");
                        continue;
                    }
                    entry.evoFields["size"] = entry.data.size();
                    if (is_evo_tokenized_entry(entry) || is_legacy_numeric_entry(entry))
                    {
                        entry.evoFields["arraylen"] = entry.data.size() / 2;
                    }
                }
            }
            this->computedChecksum = this->computeEvoChecksumFromInstanceData();
            return;
        }

        this->header.entries_len = 0;
        for (auto& entry : this->entries)
        {
            entry.determineFullType();
            if (is_backup_entry(entry))
            {
                const auto backup = TypeHandlers::TH_Backup::parseInternal(entry.data);
                entry.meta_length = backup.hasData4 ? TypeHandlers::TH_Backup::onFileMetaLength4 : TypeHandlers::TH_Backup::onFileMetaLength3;
                entry.data_length = entry.data_length2 = static_cast<uint16_t>(backup.data1.size());
                this->header.entries_len += sizeof(var_entry_t::meta_length)
                                         + entry.meta_length
                                         + sizeof(var_entry_t::data_length2)
                                         + backup.data1.size()
                                         + backup.data2.size()
                                         + backup.data3.size()
                                         + backup.data4.size();
                entry.version = VER_NONE;
                entry.archivedFlag = 0;
            }
            else
            {
                entry.data_length2 = entry.data_length = (uint16_t) entry.data.size();
                this->header.entries_len += sizeof(var_entry_t::data_length) + sizeof(var_entry_t::data_length2) + entry.meta_length + entry.data_length;
                entry.version = (this->calcModel.getFlags() & TIFeatureFlags::hasFlash) ? std::get<2>(entry._type.getHandlers())(entry.data) : VER_NONE;
            }
        }
        this->computedChecksum = this->computeChecksumFromInstanceData();
    }

    void TIVarFile::checkVarEntriesVersionCompat() const
    {
        if (this->header.ownerPID > 0 && this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
        {
            uint8_t max_version = 0;
            for (const auto& entry : this->entries)
            {
                const uint8_t maskedVersion = entry.version & ~0x20;
                if (maskedVersion > max_version) max_version = maskedVersion;
            }
            if (this->header.ownerPID < max_version)
            {
                printf("[Warning] The declared calculator model (owner ID 0x%02X) may not support the var entry version 0x%02X found in this file.\n", this->header.ownerPID, max_version);
            }
        }
    }

    void TIVarFile::var_entry_t::setVarName(const std::string &name) {
        std::string newName(name);
        if (newName.empty())
        {
            newName = default_var_name_for_type(_type);
        }

        // Here we handle various theta chars. Note thata in TI-ASCII, theta is at 0x5B which is "[" in ASCII.
        newName = normalize_theta_chars(newName);

        const auto& typeName = _type.getName();
        const auto& typeId = _type.getId();

        if (typeName == "Real" || typeName == "Complex" || typeName == "Program" || typeName == "ProtectedProgram")
        {
            newName = uppercase_ascii(newName);
        }

        bool isValid = true;
        if (typeName == "Real" || typeName == "Complex" || (typeId >= 0x1B && typeId <= 0x21))
        {
            isValid = newName.size() == 1 && is_alpha_or_theta(newName[0]);
        }
        else if (typeName == "RealList" || typeName == "ComplexList")
        {
            isValid = normalize_list_name(newName);
        }
        else if (typeName == "Program" || typeName == "ProtectedProgram")
        {
            isValid = !newName.empty() && newName.size() <= sizeof(varname) && is_alpha_or_theta(newName[0]);
            if (isValid)
            {
                for (const char c : newName)
                {
                    if (!is_name_char(c))
                    {
                        isValid = false;
                        break;
                    }
                }
            }
        }
        else if (typeName == "Equation" || typeName == "SmartEquation")
        {
            isValid = normalize_equation_name(newName);
        }
        else if (typeName == "Matrix")
        {
            isValid = normalize_matrix_name(newName);
        }
        else if (typeName == "String")
        {
            isValid = normalize_decimal_slot_name(newName, "STR", 0xAA);
        }
        else if (typeName == "Picture")
        {
            isValid = normalize_decimal_slot_name(newName, "PIC", 0x60);
        }
        else if (typeName == "Image")
        {
            isValid = normalize_decimal_slot_name(newName, "IMAGE", 0x3C);
        }
        else if (typeName == "GraphDataBase")
        {
            isValid = normalize_decimal_slot_name(newName, "GDB", 0x61);
        }
        else if (typeName == "TableRange")
        {
            isValid = normalize_fixed_ascii_name(newName, "TblSet");
        }
        else if (typeName == "WindowSettings")
        {
            isValid = normalize_fixed_ascii_name(newName, "Window");
        }
        else if (typeName == "RecallWindow")
        {
            isValid = normalize_fixed_ascii_name(newName, "RclWindw");
        }

        if (!isValid)
        {
            newName.clear();
        }

        if (newName.length() > sizeof(varname) || newName.empty())
        {
            throw std::invalid_argument("Invalid name given. 8 chars (A-Z, 0-9, θ) max, starting by a letter or θ.");
        }

        newName = str_pad(newName, sizeof(varname), "\0");
        std::ranges::copy(newName, varname);
    }

    void TIVarFile::var_entry_t::determineFullType()
    {
        _type = TIVarType{typeID};
        if (_type.getName() == "AppVar")
        {
            const std::string detectedTypeName = TypeHandlers::detectStructuredAppVarTypeName(data);
            if (detectedTypeName != "AppVar")
            {
                _type = TIVarType{detectedTypeName};
            }
        }
    }

        /*** Public actions **/

    /**
    * @param    array   data   The array of bytes
    */
    void TIVarFile::setContentFromData(const data_t& data, uint16_t entryIdx)
    {
        if (!data.empty())
        {
            this->entries[entryIdx].data = data;
            this->refreshMetadataFields();
        } else {
            throw std::runtime_error("[Error] No data given");
        }
    }
    void TIVarFile::setContentFromData(const data_t& data)
    {
        setContentFromData(data, 0);
    }

    void TIVarFile::setContentFromString(const std::string& str, const options_t& options, uint16_t entryIdx)
    {
        auto& entry = this->entries[entryIdx];
        data_t data = std::get<0>(entry._type.getHandlers())(str, options, this);
        if (this->evoFormat && is_evo_tokenized_entry(entry))
        {
            data = legacy_tokenized_data_to_evo(data);
        }
        else if (this->evoFormat && is_legacy_numeric_entry(entry))
        {
            const bool exactFraction = legacy_value_is_exact_fraction(data);
            data = legacy_value_to_evo_expression(data);
            entry.evoFields["flags"] = exactFraction ? 6 : 0;
        }
        else if (this->evoFormat && (entry._type.getName() == "RealList" || entry._type.getName() == "ComplexList"))
        {
            data = legacy_list_to_evo(data, entry._type.getName() == "ComplexList", entry.evoFields);
        }
        else if (this->evoFormat && entry._type.getName() == "Matrix")
        {
            data = legacy_matrix_to_evo(data, entry.evoFields);
        }
        else if (this->evoFormat && entry._type.getName() == "Picture")
        {
            data = legacy_picture_to_evo(data, entry.evoFields);
        }
        else if (this->evoFormat && entry._type.getName() == "Image")
        {
            data = legacy_image_to_evo(data, entry.evoFields);
        }
        else if (this->evoFormat && entry._type.getName().find("AppVar") != std::string::npos)
        {
            entry.evoFields["version"] = 1;
        }
        entry.data = data;
        this->refreshMetadataFields();
    }
    void TIVarFile::setContentFromString(const std::string& str, const options_t& options)
    {
        setContentFromString(str, options, 0);
    }
    void TIVarFile::setContentFromString(const std::string& str)
    {
        setContentFromString(str, {}, 0);
    }

    void TIVarFile::setCalcModel(const TIModel& model)
    {
        this->calcModel = model;
        std::string signature = model.getSig();
        std::ranges::copy(signature, this->header.signature);
    }

    void TIVarFile::convertToModel(const TIModel& model)
    {
        const bool targetEvoFormat = (model.getFlags() & TIFeatureFlags::hasEvoASIC) != 0;
        if (!model_supports_all_entries(model, this->entries))
        {
            throw std::runtime_error("This calculator model (" + model.getName() + ") does not support every variable entry in this file");
        }

        if (targetEvoFormat && this->entries.size() != 1)
        {
            throw std::runtime_error("Evo variable files can only contain one variable entry");
        }

        if (this->evoFormat != targetEvoFormat)
        {
            for (auto& entry : this->entries)
            {
                if (targetEvoFormat)
                {
                    const uint8_t evoTypeID = evo_type_from_type(entry._type, entry.evoTypeID);
                    const std::string displayName = entry_name_to_string(entry._type, entry.varname, sizeof(var_entry_t::varname));

                    entry.evoFields.clear();
                    if (is_evo_tokenized_entry(entry))
                    {
                        entry.data = legacy_tokenized_data_to_evo(entry.data);
                        entry.evoFields["version"] = 1;
                        entry.evoFields["arraylen"] = entry.data.size() / 2;
                        entry.evoFields["size"] = entry.data.size();
                    }
                    else if (is_legacy_numeric_entry(entry))
                    {
                        const bool exactFraction = legacy_value_is_exact_fraction(entry.data);
                        entry.data = legacy_value_to_evo_expression(entry.data);
                        entry.evoFields["version"] = 1;
                        entry.evoFields["flags"] = exactFraction ? 6 : 0;
                        entry.evoFields["arraylen"] = entry.data.size() / 2;
                        entry.evoFields["size"] = entry.data.size();
                    }
                    else if (entry._type.getName() == "RealList" || entry._type.getName() == "ComplexList")
                    {
                        entry.data = legacy_list_to_evo(entry.data, entry._type.getName() == "ComplexList", entry.evoFields);
                    }
                    else if (entry._type.getName() == "Matrix")
                    {
                        entry.data = legacy_matrix_to_evo(entry.data, entry.evoFields);
                    }
                    else if (entry._type.getName() == "Picture")
                    {
                        entry.data = legacy_picture_to_evo(entry.data, entry.evoFields);
                    }
                    else if (entry._type.getName() == "Image")
                    {
                        entry.data = legacy_image_to_evo(entry.data, entry.evoFields);
                    }
                    else if (entry._type.getName().find("AppVar") != std::string::npos)
                    {
                        entry.evoFields["version"] = 1;
                        entry.evoFields["size"] = entry.data.size();
                    }
                    else
                    {
                        throw std::runtime_error("Evo conversion is not supported for " + entry._type.getName() + " variables");
                    }

                    entry.evoTypeID = evoTypeID;
                    entry.evoMetaVersion = entry.evoMetaVersion == 0 ? 1 : entry.evoMetaVersion;
                    entry.evoMetaFlags = (evoTypeID == 5 || evoTypeID == 4) ? 1 : entry.evoMetaFlags;
                    entry.evoNameBytes = encode_evo_name(evoTypeID, displayName);
                    entry.evoDataIsRawCBOR = false;
                    entry.meta_length = 0;
                }
                else
                {
                    if (is_evo_tokenized_entry(entry))
                    {
                        entry.data = evo_tokenized_data_to_legacy(entry.data);
                    }
                    else if (entry.evoTypeID == 0)
                    {
                        size_t offset = 0;
                        entry.data = evo_scalar_to_legacy_value(entry.data, offset);
                        set_numeric_entry_type_from_payload(entry);
                    }
                    else if (entry.evoTypeID == 1)
                    {
                        const auto lenIt = entry.evoFields.find("len");
                        if (lenIt == entry.evoFields.end())
                        {
                            throw std::runtime_error("Invalid Evo list metadata: missing len");
                        }
                        bool complexList = false;
                        entry.data = evo_list_to_legacy(entry.data, lenIt->second, complexList);
                        set_entry_type(entry, TIVarType{complexList ? "ComplexList" : "RealList"});
                    }
                    else if (entry.evoTypeID == 6)
                    {
                        const auto rowsIt = entry.evoFields.find("rows");
                        const auto colsIt = entry.evoFields.find("cols");
                        if (rowsIt == entry.evoFields.end() || colsIt == entry.evoFields.end())
                        {
                            throw std::runtime_error("Invalid Evo matrix metadata: missing dimensions");
                        }
                        entry.data = evo_matrix_to_legacy(entry.data, rowsIt->second, colsIt->second);
                        set_entry_type(entry, TIVarType{"Matrix"});
                    }
                    else if (entry.evoTypeID == 5)
                    {
                        entry.data = evo_image_to_legacy(entry.data);
                        set_entry_type(entry, TIVarType{"Image"});
                    }
                    else if (entry.evoTypeID == 4)
                    {
                        entry.data = evo_picture_to_legacy(entry.data);
                        set_entry_type(entry, TIVarType{"Picture"});
                    }
                    else if (entry.evoTypeID == 8 && !entry.evoDataIsRawCBOR)
                    {
                        set_entry_type(entry, TIVarType{"AppVar"});
                        entry.determineFullType();
                    }
                    else
                    {
                        throw std::runtime_error("Evo conversion is not supported for type " + std::to_string(entry.evoTypeID) + " variables");
                    }

                    const std::string displayName = decode_evo_name(entry.evoTypeID, entry.evoNameBytes);
                    if (!displayName.empty())
                    {
                        entry.setVarName(displayName);
                    }
                    entry.evoFields.clear();
                    entry.evoNameBytes.clear();
                    entry.evoDataIsRawCBOR = false;
                    entry.meta_length = (model.getFlags() & TIFeatureFlags::hasFlash) ? varEntryNewLength : varEntryOldLength;
                    entry.evoTypeID = 0;
                    entry.evoMetaVersion = 1;
                    entry.evoMetaFlags = 0;
                }
            }
        }

        this->evoFormat = targetEvoFormat;
        this->setCalcModel(model);
        this->refreshMetadataFields();
    }

    void TIVarFile::setVarName(const std::string& name, uint16_t entryIdx)
    {
        this->entries[entryIdx].setVarName(name);
        this->refreshMetadataFields();
    }
    void TIVarFile::setVarName(const std::string& name)
    {
        this->setVarName(name, 0);
    }

    void TIVarFile::setArchived(bool flag, uint16_t entryIdx)
    {
        if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
        {
            this->entries[entryIdx].setArchived(flag);
            this->refreshMetadataFields();
        } else {
            throw std::runtime_error("[Error] Archived flag not supported on this calculator model");
        }
    }
    void TIVarFile::setArchived(bool flag)
    {
        this->setArchived(flag, 0);
    }


    bool TIVarFile::isCorrupt() const
    {
        return corrupt;
    }

    data_t TIVarFile::getRawContent(uint16_t entryIdx)
    {
        return this->entries[entryIdx].data;
    }
    data_t TIVarFile::getRawContent()
    {
        return this->getRawContent(0);
    }

    std::string TIVarFile::getRawContentHexStr()
    {
        const data_t rawContent = getRawContent();
        std::ostringstream result;
        for (const auto& v : rawContent)
        {
            result << std::setfill('0') << std::setw(sizeof(v) * 2) << std::hex << +v;
        }
        return result.str();
    }

    std::string TIVarFile::getReadableContent(const options_t& options, uint16_t entryIdx) const
    {
        if (this->evoFormat)
        {
            return getEvoReadableContent(options, entryIdx);
        }
        const auto& entry = this->entries[entryIdx];
        return std::get<1>(entry._type.getHandlers())(entry.data, options, this);
    }
    std::string TIVarFile::getReadableContent(const options_t& options) const
    {
        return getReadableContent(options, 0);
    }
    std::string TIVarFile::getReadableContent() const
    {
        return getReadableContent({}, 0);
    }

    data_t TIVarFile::make_bin_data()
    {
        if (this->evoFormat)
        {
            return make_evo_bin_data();
        }

        data_t bin_data;
        bin_data.reserve(firstVarEntryOffset + this->header.entries_len);

        // Header
        {
            bin_data.insert(bin_data.end(), this->header.signature, this->header.signature + sizeof(var_header_t::signature));
            bin_data.insert(bin_data.end(), this->header.sig_extra, this->header.sig_extra + sizeof(var_header_t::sig_extra));
            bin_data.push_back(this->header.ownerPID);
            bin_data.insert(bin_data.end(), this->header.comment,   this->header.comment   + sizeof(var_header_t::comment));
            bin_data.push_back((uint8_t) (this->header.entries_len & 0xFF)); bin_data.push_back((uint8_t) ((this->header.entries_len >> 8) & 0xFF));
        }

        // Var entries
        for (const auto& entry : this->entries)
        {
            if (is_backup_entry(entry))
            {
                const auto backup = TypeHandlers::TH_Backup::parseInternal(entry.data);
                const uint16_t metaLength = backup.hasData4 ? TypeHandlers::TH_Backup::onFileMetaLength4 : TypeHandlers::TH_Backup::onFileMetaLength3;
                const uint16_t data1Length = static_cast<uint16_t>(backup.data1.size());
                const uint16_t data2Length = static_cast<uint16_t>(backup.data2.size());
                const uint16_t data3Length = static_cast<uint16_t>(backup.data3.size());
                const uint16_t data4Length = static_cast<uint16_t>(backup.data4.size());

                bin_data.push_back((uint8_t) (metaLength & 0xFF));
                bin_data.push_back((uint8_t) ((metaLength >> 8) & 0xFF));
                bin_data.push_back((uint8_t) (data1Length & 0xFF));
                bin_data.push_back((uint8_t) ((data1Length >> 8) & 0xFF));
                bin_data.push_back(entry.typeID);
                bin_data.push_back((uint8_t) (data2Length & 0xFF));
                bin_data.push_back((uint8_t) ((data2Length >> 8) & 0xFF));
                bin_data.push_back((uint8_t) (data3Length & 0xFF));
                bin_data.push_back((uint8_t) ((data3Length >> 8) & 0xFF));
                bin_data.push_back((uint8_t) (backup.addressOfData2 & 0xFF));
                bin_data.push_back((uint8_t) ((backup.addressOfData2 >> 8) & 0xFF));
                if (backup.hasData4)
                {
                    bin_data.push_back((uint8_t) (data4Length & 0xFF));
                    bin_data.push_back((uint8_t) ((data4Length >> 8) & 0xFF));
                }
                bin_data.push_back((uint8_t) (data1Length & 0xFF));
                bin_data.push_back((uint8_t) ((data1Length >> 8) & 0xFF));
                bin_data.insert(bin_data.end(), backup.data1.begin(), backup.data1.end());
                bin_data.insert(bin_data.end(), backup.data2.begin(), backup.data2.end());
                bin_data.insert(bin_data.end(), backup.data3.begin(), backup.data3.end());
                if (backup.hasData4)
                {
                    bin_data.insert(bin_data.end(), backup.data4.begin(), backup.data4.end());
                }
                continue;
            }

            bin_data.push_back((uint8_t) (entry.meta_length & 0xFF)); bin_data.push_back((uint8_t) ((entry.meta_length >> 8) & 0xFF));
            bin_data.push_back((uint8_t) (entry.data_length & 0xFF)); bin_data.push_back((uint8_t) ((entry.data_length >> 8) & 0xFF));
            bin_data.push_back(entry.typeID);
            bin_data.insert(bin_data.end(), entry.varname, entry.varname + + sizeof(var_entry_t::varname));
            if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
            {
                bin_data.push_back(entry.version);
                bin_data.push_back(entry.archivedFlag);
            }
            bin_data.push_back((uint8_t) (entry.data_length2 & 0xFF)); bin_data.push_back((uint8_t) ((entry.data_length2 >> 8) & 0xFF));
            bin_data.insert(bin_data.end(), entry.data.begin(), entry.data.end());
        }

        return bin_data;
    }

    data_t TIVarFile::make_evo_bin_data() const
    {
        if (this->entries.empty())
        {
            throw std::runtime_error("No Evo variable entry to serialize");
        }

        const auto& entry = this->entries[0];
        const uint8_t evoTypeID = evo_type_from_type(entry._type, entry.evoTypeID);
        const std::string displayName = entry_name_to_string(entry._type, entry.varname, sizeof(var_entry_t::varname));
        const data_t nameBytes = !entry.evoNameBytes.empty() ? entry.evoNameBytes : encode_evo_name(evoTypeID, displayName);

        data_t out;
        out.push_back(0xBF);
        append_cbor_text(out, "metaData");
        out.push_back(0xBF);
        append_cbor_key_uint(out, "type", evoTypeID);
        append_cbor_key_uint(out, "version", entry.evoMetaVersion == 0 ? 1 : entry.evoMetaVersion);
        append_cbor_key_uint(out, "flags", entry.evoMetaFlags);
        append_cbor_text(out, "name");
        append_cbor_bytes(out, nameBytes);
        out.push_back(0xFF);

        const std::vector<std::string> preferredFieldOrder = {
            "version", "flags", "rows", "cols", "type", "len", "arraylen", "size"
        };
        std::unordered_set<std::string> emitted;
        for (const std::string& key : preferredFieldOrder)
        {
            const auto it = entry.evoFields.find(key);
            if (it != entry.evoFields.end())
            {
                append_cbor_key_uint(out, key, it->second);
                emitted.insert(key);
            }
        }
        for (const auto& [key, value] : entry.evoFields)
        {
            if (!emitted.contains(key))
            {
                append_cbor_key_uint(out, key, value);
            }
        }

        if (!entry.data.empty() || entry.evoDataIsRawCBOR || entry.evoFields.contains("size"))
        {
            append_cbor_text(out, "data");
            if (entry.evoDataIsRawCBOR)
            {
                out.insert(out.end(), entry.data.begin(), entry.data.end());
            }
            else
            {
                append_cbor_bytes(out, entry.data);
            }
        }

        out.push_back(0xFF);
        return out;
    }

    std::string TIVarFile::getEvoReadableContent(const options_t& options, uint16_t entryIdx) const
    {
        (void)options;
        const auto& entry = this->entries[entryIdx];
        nlohmann::ordered_json j;
        j["type"] = entry.evoTypeID;
        j["typeName"] = type_name_from_evo_type(entry.evoTypeID);
        j["name"] = decode_evo_name(entry.evoTypeID, entry.evoNameBytes.empty() ? encode_evo_name(entry.evoTypeID, entry_name_to_string(entry._type, entry.varname, sizeof(var_entry_t::varname))) : entry.evoNameBytes);
        j["metaData"] = {
            {"version", entry.evoMetaVersion},
            {"flags", entry.evoMetaFlags},
            {"nameHex", bytes_to_hex_string(entry.evoNameBytes)},
        };
        for (const auto& [key, value] : entry.evoFields)
        {
            j[key] = value;
        }
        j["rawDataHex"] = bytes_to_hex_string(entry.data);
        if (!entry.evoDataIsRawCBOR && (entry.evoTypeID == 2 || entry.evoTypeID == 7))
        {
            j["code"] = detokenize_evo_token_words(entry.data);
        }
        if (!entry.evoDataIsRawCBOR && entry.evoTypeID == 15)
        {
            try
            {
                const EvoPythonScriptInfo python = parse_evo_python_script_payload(entry.data);
                j["python"] = {
                    {"magicHex", bytes_to_hex_string(python.magic)},
                    {"bodyLen", python.bodyLen},
                    {"nameLen", python.nameLen},
                    {"name", python.name},
                    {"scriptLen", python.scriptLen},
                    {"scriptType", python.scriptType},
                    {"trailerHex", bytes_to_hex_string(python.trailer)},
                    {"bodyHex", bytes_to_hex_string(python.body)},
                };
                if (python.bodyIsText)
                {
                    j["python"]["code"] = python.code;
                }
            }
            catch (const std::exception& e)
            {
                j["pythonParseError"] = e.what();
            }
        }
        return j.dump(4);
    }

    /**
     * Writes a variable to an actual file on the FS
     * If the variable was already loaded from a file, it will be used and overwritten,
     * except if a specific directory and name are provided.
     *
     * @param   string  directory  Directory to save the file to
     * @param   string  name       Name of the file, without the extension
     * @return  string  the full path
     */
    std::string TIVarFile::saveVarToFile(std::string directory, std::string name)
    {
        std::string fullPath;

        if (this->fromFile && directory.empty())
        {
            fullPath = this->filePath;
        } else {
            if (name.empty())
            {
                name = this->hasMultipleEntries() ? "GROUP" : entry_name_to_string(this->entries[0]._type, this->entries[0].varname, sizeof(var_entry_t::varname));
                name = sanitize_for_host_filename(name);
            }
            std::string fileName;
            if (this->hasMultipleEntries())
            {
                fileName = name + ".8xg";
            } else {
                if (this->evoFormat)
                {
                    const std::string evoExt = extension_from_evo_type(this->entries[0].evoTypeID);
                    fileName = name + "." + (evoExt.empty() ? "8x?2" : evoExt);
                }
                else
                {
                    const int extIndex = std::max(0, this->calcModel.getOrderId());
                    fileName = name + "." + this->entries[0]._type.getExts()[extIndex];
                }
            }
            if (directory.empty())
            {
                directory = ".";
            }
            fullPath = directory + "/" + fileName;
        }

        return saveVarToFile(fullPath);
    }

    std::string TIVarFile::saveVarToFile(std::string path)
    {
        FILE* handle = fopen(path.c_str(), "wb");
        if (!handle)
        {
            throw std::runtime_error("Can't open the output file");
        }

        this->refreshMetadataFields();

        // Make and write file data
        const data_t bin_data = make_bin_data();
        fwrite(&bin_data[0], sizeof(bin_data[0]), bin_data.size(), handle);

        // Write checksum (Evo: big-endian, pre-Evo: little-endian)
        const char lo = static_cast<char>(this->computedChecksum & 0xFF);
        const char hi = static_cast<char>((this->computedChecksum >> 8) & 0xFF);
        const char buf[2] = {this->evoFormat ? hi : lo, this->evoFormat ? lo : hi};
        fwrite(buf, sizeof(char), 2, handle);

        fclose(handle);

        this->corrupt = false;

        return path;
    }

    std::string TIVarFile::saveVarToFile()
    {
        return saveVarToFile("", "");
    }
}

#ifdef __EMSCRIPTEN__
    #include <emscripten/bind.h>
    using namespace emscripten;

    namespace
    {
        tivars::TIVarFile createNewFromStrings(const std::string& type, const std::string& name, const std::string& model)
        {
            return tivars::TIVarFile::createNew(tivars::TIVarType{type}, name, tivars::TIModel{model});
        }

        tivars::TIVarFile createNewFromStrings(const std::string& type, const std::string& name)
        {
            return tivars::TIVarFile::createNew(tivars::TIVarType{type}, name);
        }

        tivars::TIVarFile createNewFromString(const std::string& type)
        {
            return tivars::TIVarFile::createNew(tivars::TIVarType{type});
        }
    }

    EMSCRIPTEN_BINDINGS(_tivarfile) {

            register_map<std::string, int>("options_t");
            register_vector<uint8_t>("data_t");

            class_<tivars::TIVarFile>("TIVarFile")
                    .function("getHeader"                , &tivars::TIVarFile::getHeader)
                    .function("getVarEntries"            , &tivars::TIVarFile::getVarEntries)
                    .function("getInstanceChecksum"      , &tivars::TIVarFile::getInstanceChecksum)

                    .function("getChecksumValueFromFile" , &tivars::TIVarFile::getChecksumValueFromFile)
                    .function("setContentFromData"       , select_overload<void(const data_t& data)>(&tivars::TIVarFile::setContentFromData))
                    .function("setContentFromString"     , select_overload<void(const std::string&, const options_t&)>(&tivars::TIVarFile::setContentFromString))
                    .function("setContentFromString"     , select_overload<void(const std::string&)>(&tivars::TIVarFile::setContentFromString))
                    .function("getCalcModel"             , &tivars::TIVarFile::getCalcModel)
                    .function("setCalcModel"             , &tivars::TIVarFile::setCalcModel)
                    .function("isEvoFormat"              , &tivars::TIVarFile::isEvoFormat)
                    .function("convertToModel"           , select_overload<void(const std::string&)>(&tivars::TIVarFile::convertToModel))
                    .function("setVarName"               , select_overload<void(const std::string&)>(&tivars::TIVarFile::setVarName))
                    .function("setArchived"              , select_overload<void(bool)>(&tivars::TIVarFile::setArchived))
                    .function("isCorrupt"                , &tivars::TIVarFile::isCorrupt)
                    .function("getRawContent"            , select_overload<data_t(void)>(&tivars::TIVarFile::getRawContent))
                    .function("getRawContentHexStr"      , &tivars::TIVarFile::getRawContentHexStr)
                    .function("getReadableContent"       , select_overload<std::string(const options_t&)const>(&tivars::TIVarFile::getReadableContent))
                    .function("getReadableContent"       , select_overload<std::string(void)const>(&tivars::TIVarFile::getReadableContent))

                    .function("saveVarToFile"            , select_overload<std::string(std::string, std::string)>(&tivars::TIVarFile::saveVarToFile))
                    .function("saveVarToFile"            , select_overload<std::string(std::string)>(&tivars::TIVarFile::saveVarToFile))
                    .function("saveVarToFile"            , select_overload<std::string(void)>(&tivars::TIVarFile::saveVarToFile))

                    .class_function("loadFromFile", &tivars::TIVarFile::loadFromFile, return_value_policy::take_ownership())
                    .class_function("createNew", select_overload<tivars::TIVarFile(const std::string&, const std::string&, const std::string&)>(&createNewFromStrings), return_value_policy::take_ownership())
                    .class_function("createNew", select_overload<tivars::TIVarFile(const std::string&, const std::string&)>(&createNewFromStrings), return_value_policy::take_ownership())
                    .class_function("createNew", &createNewFromString, return_value_policy::take_ownership())
            ;
    }
#endif
