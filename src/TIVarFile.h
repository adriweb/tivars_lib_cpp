/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARS_LIB_CPP_TIVARFILE_H
#define TIVARS_LIB_CPP_TIVARFILE_H

#include "CommonTypes.h"
#include "BinaryFile.h"
#include "TIVarType.h"
#include "TIModel.h"

namespace tivars
{

    class TIVarFile : public BinaryFile
    {
        // For the record, 83+ = 4, 84+ = 10, 82A = 11, 84+CSE = 15, CE = 19, 84+T = 27.
        static const constexpr uint8_t OWNER_PID_NONE = 0;

#pragma pack(push, 1)
        struct var_header_t
        {
            uint8_t  signature[8]  = {};
            uint8_t  sig_extra[3]  = { 0x1A, 0x0A, OWNER_PID_NONE };
            uint8_t  comment[42]   = {};
            uint16_t entries_len   = 0;
        };
#pragma pack(pop)

#pragma pack(push, 1)
        struct var_entry_t
        {
            uint16_t meta_length   = 0; // byte count of the next 3 or 5 fields (== 11 or 13) depending on calcFlags, see below
            uint16_t data_length   = 0;
            uint8_t  typeID        = 0;
            uint8_t  varname[8]    = {};
            uint8_t  version       = 0; // present only if calcFlags >= TIFeatureFlags::hasFlash
            uint8_t  archivedFlag  = 0; // present only if calcFlags >= TIFeatureFlags::hasFlash
            uint16_t data_length2  = 0; // same as data_length
            data_t   data;
        };
#pragma pack(pop)

        static const constexpr uint16_t dataSectionOffset = sizeof(var_header_t); // comes right after the header, so == its size
        static_assert(dataSectionOffset == 55, "dataSectionOffset size needs to be 55");

        static const constexpr uint16_t varEntryNewLength = sizeof(var_entry_t) - sizeof(var_entry_t::meta_length) - sizeof(var_entry_t::data_length2) - sizeof(var_entry_t::data);
        static_assert(varEntryNewLength == 0x0D, "varEntryNewLength size needs to be 13");

        static const constexpr uint16_t varEntryOldLength = varEntryNewLength - sizeof(var_entry_t::version) - sizeof(var_entry_t::archivedFlag);
        static_assert(varEntryOldLength == 0x0B, "varEntryOldLength size needs to be 11");

    public:
        TIVarFile() = delete;

        const var_header_t& getHeader() const { return header; }
        const var_entry_t& getVarEntry() const { return varEntry; }
        const TIVarType& getType() const { return type; }
        uint16_t getInstanceChecksum() const { return computedChecksum; }

        static TIVarFile loadFromFile(const std::string& filePath);

        static TIVarFile createNew(const TIVarType& type, const std::string& name, const TIModel& model);
        static TIVarFile createNew(const TIVarType& type, const std::string& name);
        static TIVarFile createNew(const TIVarType& type);

        void makeHeaderFromFile();

        void makeVarEntryFromFile();

        uint16_t getChecksumValueFromFile();

        void setContentFromData(const data_t& data);

        void setContentFromString(const std::string& str, const options_t& options);
        void setContentFromString(const std::string& str);

        void setCalcModel(const TIModel& model);
        void setVarName(const std::string& name);
        void setArchived(bool flag);

        bool isCorrupt() const;

        data_t getRawContent();

        std::string getReadableContent(const options_t& options);
        std::string getReadableContent();

        std::string saveVarToFile(std::string directory, std::string name);
        std::string saveVarToFile(std::string path);
        std::string saveVarToFile();

    private:
        TIVarFile(const TIVarType& type, const std::string& name, const TIModel& model);
        explicit TIVarFile(const std::string& filePath);

        void refreshMetadataFields();
        std::string fixVarName(const std::string& name);

        uint16_t computeChecksumFromInstanceData();
        uint16_t computeChecksumFromFileData();

        // Extends BinaryFile.
        uint16_t get_two_bytes_swapped()
        {
            return (this->get_raw_byte() & 0xFF) + (this->get_raw_byte() << 8);
        }

        var_header_t header;
        var_entry_t  varEntry;
        TIVarType    type;
        TIModel      calcModel;
        uint16_t     computedChecksum = 0;
        uint16_t     fileChecksum     = 0;
        bool         fromFile         = false;
        bool         corrupt          = false;

        data_t make_bin_data();

    };
}

#endif //TIVARS_LIB_CPP_TIVARFILE_H
