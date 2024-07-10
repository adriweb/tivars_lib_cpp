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
    public:
        // For the record, 83+ = 4, 84+ = 10, 82A = 11, 84+CSE = 15, CE = 19, 84+T = 27...
        static const constexpr uint8_t OWNER_PID_NONE = 0;

        struct var_header_t
        {
            uint8_t  signature[8]  = {};
            uint8_t  sig_extra[2]  = { 0x1A, 0x0A }; // this never actually changes
            uint8_t  ownerPID      = OWNER_PID_NONE; // informational - may reflect what's on the version field in the var entries
            uint8_t  comment[42]   = {};
            uint16_t entries_len   = 0;
        };

        struct var_entry_t
        {
            uint16_t meta_length = 0; // byte count of the next 3 or 5 fields (== 11 or 13) depending on calcFlags, see below
            uint16_t data_length   = 0;
            uint8_t  typeID        = 0;
            uint8_t  varname[8]    = {};
            uint8_t  version       = 0; // present only if calcFlags >= TIFeatureFlags::hasFlash
            uint8_t  archivedFlag  = 0; // present only if calcFlags >= TIFeatureFlags::hasFlash
            uint16_t data_length2 = 0; // same as data_length
            data_t data;

            TIVarType _type{}; // has "full" type information (useful for appvars that have a "sub"type)
            void setArchived(bool archived) { archivedFlag = (archived ? 0x80 : 0); }
            void setVarName(const std::string& name);
            void determineFullType(void);
        };

        // comes right after the var header, so == its size
        static const constexpr uint16_t firstVarEntryOffset = sizeof(var_header_t::signature) + sizeof(var_header_t::sig_extra) + sizeof(var_header_t::ownerPID) + sizeof(var_header_t::comment) + sizeof(var_header_t::entries_len);
        static_assert(firstVarEntryOffset == 55, "firstVarEntryOffset size needs to be 55");

        static const constexpr uint16_t varEntryOldLength = sizeof(var_entry_t::data_length) + sizeof(var_entry_t::typeID) + sizeof(var_entry_t::varname);
        static_assert(varEntryOldLength == 0x0B, "varEntryOldLength size needs to be 11");

        static const constexpr uint16_t varEntryNewLength = varEntryOldLength + sizeof(var_entry_t::version) + sizeof(var_entry_t::archivedFlag);
        static_assert(varEntryNewLength == 0x0D, "varEntryNewLength size needs to be 13");

        TIVarFile() = delete;

        const var_header_t& getHeader() const { return header; }
        const std::vector<var_entry_t>& getVarEntries() const { return entries; }
        uint16_t getInstanceChecksum() const { return computedChecksum; }
        bool hasMultipleEntries() const { return entries.size() > 1; }

        static TIVarFile loadFromFile(const std::string& filePath);

        static TIVarFile createNew(const TIVarType& type, const std::string& name, const TIModel& model);
        static TIVarFile createNew(const TIVarType& type, const std::string& name);
        static TIVarFile createNew(const TIVarType& type);

        // Additional overloads for easier Emscripten usage
#ifdef __EMSCRIPTEN__
        static TIVarFile createNew(const std::string& type, const std::string& name, const std::string& model)
        {
            return TIVarFile{TIVarType{type}, name, TIModel{model}};
        }
        static TIVarFile createNew(const std::string& type, const std::string& name)
        {
            return createNew(TIVarType{type}, name);
        }
        static TIVarFile createNew(const std::string& type)
        {
            return createNew(TIVarType{type});
        }
#endif

        uint16_t getChecksumValueFromFile();

        void setContentFromData(const data_t& data, uint16_t entryIdx);
        void setContentFromData(const data_t& data);

        void setContentFromString(const std::string& str, const options_t& options, uint16_t entryIdx);
        void setContentFromString(const std::string& str, const options_t& options);
        void setContentFromString(const std::string& str);

        void setCalcModel(const TIModel& model);

        void setVarName(const std::string& name, uint16_t entryIdx);
        void setVarName(const std::string& name);
        void setArchived(bool flag, uint16_t entryIdx);
        void setArchived(bool flag);

        bool isCorrupt() const;

        data_t getRawContent(uint16_t entryIdx);
        data_t getRawContent();

        std::string getRawContentHexStr();

        std::string getReadableContent(const options_t& options, uint16_t entryIdx);
        std::string getReadableContent(const options_t& options);
        std::string getReadableContent();

        std::string saveVarToFile(std::string directory, std::string name);
        std::string saveVarToFile(std::string path);
        std::string saveVarToFile();

    private:
        TIVarFile(const TIVarType& type, const std::string& name, const TIModel& model);
        explicit TIVarFile(const std::string& filePath);

        void refreshMetadataFields();
        void checkVarEntriesVersionCompat() const;

        void makeHeaderFromFile();
        void makeVarEntriesFromFile();

        uint16_t computeChecksumFromInstanceData();
        uint16_t computeChecksumFromFileData();

        // Extends BinaryFile.
        uint16_t get_two_bytes_swapped()
        {
            uint8_t low = this->get_raw_byte();
            uint8_t high = this->get_raw_byte();
            return low + (high << 8);
        }

        var_header_t header;
        std::vector<var_entry_t> entries;
        TIModel      calcModel;
        uint16_t     computedChecksum = 0;
        uint16_t     fileChecksum     = 0;
        bool         fromFile         = false;
        bool         corrupt          = false;

        data_t make_bin_data();

    };
}

#endif //TIVARS_LIB_CPP_TIVARFILE_H
