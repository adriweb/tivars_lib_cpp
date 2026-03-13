/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARS_LIB_CPP_TIFLASHFILE_H
#define TIVARS_LIB_CPP_TIFLASHFILE_H

#include "CommonTypes.h"
#include "TIModel.h"
#include "TIVarType.h"

namespace tivars
{
    class TIFlashFile
    {
    public:
        struct flash_date_t
        {
            uint8_t day = 0;
            uint8_t month = 0;
            uint16_t year = 0;
        };

        struct flash_block_t
        {
            uint16_t address = 0;
            uint8_t blockType = 0x00;
            data_t data;
        };

        struct flash_header_t
        {
            std::string magic = magicStr;
            std::string revision = "0.0";
            uint8_t binaryFlag = intelDataFlag;
            uint8_t objectType = 0x00;
            flash_date_t date{};
            std::string name = "UNNAMED";
            std::vector<std::pair<uint8_t, uint8_t>> devices;
            uint8_t productId = 0;
            data_t calcData;
            bool hasChecksum = true;
            TIVarType type{};
            TIModel model{};
        };

        static constexpr char magicStr[] = "**TIFL**";
        static constexpr uint8_t rawBinaryDataFlag = 0x00;
        static constexpr uint8_t intelDataFlag = 0x01;
        static constexpr uint8_t defaultDeviceType = 0x73;
        static constexpr size_t magicByteCount = 8;
        static constexpr size_t revisionByteCount = 2;
        static constexpr size_t dateByteCount = 4;
        static constexpr size_t nameByteCount = 31;
        static constexpr size_t deviceFieldByteCount = 25;
        static constexpr size_t productIdByteCount = 1;
        static constexpr size_t dataSizeByteCount = 4;
        static constexpr size_t checksumByteCount = 2;
        static constexpr size_t headerByteCountWithoutChecksum = 78;

        TIFlashFile() = default;

        static TIFlashFile loadFromFile(const std::string& filePath);
        static TIFlashFile createNew(const TIVarType& type, const std::string& name, const TIModel& model, bool rawBinaryData = false);
        static TIFlashFile createNew(const TIVarType& type, const std::string& name, bool rawBinaryData = false);
        static TIFlashFile createNew(const TIVarType& type, bool rawBinaryData = false);

        const std::vector<flash_header_t>& getHeaders() const { return headers; }
        bool hasMultipleHeaders() const { return headers.size() > 1; }

        void addHeader(const TIVarType& type, const std::string& name, const TIModel& model, bool rawBinaryData = false);

        std::string getReadableContent(size_t headerIdx = 0) const;
        void setContentFromString(const std::string& str, size_t headerIdx = 0);

        data_t makeBinData() const;
        std::string saveToFile(const std::string& path) const;

    private:
        explicit TIFlashFile(const std::string& filePath);
        TIFlashFile(const TIVarType& type, const std::string& name, const TIModel& model, bool rawBinaryData);

        static flash_header_t parseHeader(const data_t& bytes, size_t offset, size_t* nextOffset);
        static size_t nextHeaderLength(const data_t& bytes, size_t offset);
        static data_t makeHeaderBytes(const flash_header_t& header);
        static uint16_t computeChecksum(const data_t& data);
        static std::vector<flash_block_t> parseIntelBlocks(const data_t& data);
        static data_t makeIntelData(const std::vector<flash_block_t>& blocks);
        static std::string blockToRecord(const flash_block_t& block);
        static flash_block_t recordToBlock(const std::string& record);
        static std::string extensionFor(const flash_header_t& header);
        static uint8_t defaultObjectTypeFor(const TIVarType& type);
        static data_t parseHex(const std::string& hex);
        static std::string toHex(const data_t& data);
        static uint8_t encodeBCDByte(uint8_t value);
        static uint8_t decodeBCDByte(uint8_t value);
        static flash_date_t parseBCDDate(const data_t& dateBytes);
        static data_t makeBCDDate(const flash_date_t& date);
        static std::string parseBCDRevision(const data_t& revisionBytes);
        static data_t makeBCDRevision(const std::string& revision);
        static TIModel modelFromProductId(uint8_t productId);

        std::vector<flash_header_t> headers;
        std::string filePath;
        bool fromFile = false;
    };
}

#endif
