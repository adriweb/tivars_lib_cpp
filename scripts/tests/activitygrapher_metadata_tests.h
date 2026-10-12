#ifndef ACTIVITYGRAPHER_METADATA_TESTS_H
#define ACTIVITYGRAPHER_METADATA_TESTS_H

#include "../../src/TIVarFile.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace activitygrapher_tests
{
    inline void metadata_require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    inline uint16_t metadata_le16(const data_t& data, size_t offset)
    {
        metadata_require(offset + 2 <= data.size(), "Metadata fixture is truncated");
        return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
    }

    inline void metadata_append_le16(data_t& data, uint16_t value)
    {
        data.push_back(static_cast<uint8_t>(value));
        data.push_back(static_cast<uint8_t>(value >> 8));
    }

    inline data_t metadata_read(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        metadata_require(file.good(), "Unable to read metadata fixture");
        return data_t(std::istreambuf_iterator<char>(file), {});
    }

    inline void metadata_write(const std::string& path, const data_t& data)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        metadata_require(file.good(), "Unable to write metadata fixture");
    }

    inline data_t metadata_fixture(const std::vector<uint16_t>& lengths)
    {
        const data_t realOne = {0x00, 0x80, 0x10, 0, 0, 0, 0, 0, 0};
        data_t entries;
        for (size_t i = 0; i < lengths.size(); i++)
        {
            metadata_append_le16(entries, lengths[i]);
            metadata_append_le16(entries, static_cast<uint16_t>(realOne.size()));
            entries.push_back(0x00); // Real.
            entries.push_back(static_cast<uint8_t>('A' + i));
            entries.insert(entries.end(), 7, 0);
            if (lengths[i] == 13) entries.insert(entries.end(), 2, 0);
            metadata_append_le16(entries, static_cast<uint16_t>(realOne.size()));
            entries.insert(entries.end(), realOne.begin(), realOne.end());
        }
        data_t bytes = {'*', '*', 'T', 'I', '8', '3', 'F', '*', 0x1A, 0x0A, 0};
        bytes.insert(bytes.end(), 42, 0);
        metadata_append_le16(bytes, static_cast<uint16_t>(entries.size()));
        bytes.insert(bytes.end(), entries.begin(), entries.end());
        uint16_t checksum = 0;
        for (uint8_t byte : entries) checksum += byte;
        metadata_append_le16(bytes, checksum);
        return bytes;
    }

    inline void metadata_check_saved(const std::string& path, uint16_t length, size_t entryCount)
    {
        const auto bytes = metadata_read(path);
        metadata_require(metadata_le16(bytes, 53) == bytes.size() - 57,
                         "Saved outer entry length does not match emitted bytes");
        uint16_t checksum = 0;
        for (size_t i = 55; i < bytes.size() - 2; i++) checksum += bytes[i];
        metadata_require(metadata_le16(bytes, bytes.size() - 2) == checksum,
                         "Saved metadata checksum does not match emitted bytes");
        size_t offset = 55;
        for (size_t i = 0; i < entryCount; i++)
        {
            metadata_require(metadata_le16(bytes, offset) == length,
                             "Saved entry metadata length does not match target model");
            const auto dataLength = metadata_le16(bytes, offset + 2);
            metadata_require(metadata_le16(bytes, offset + 2 + length) == dataLength,
                             "Saved entry data lengths differ");
            offset += 2 + length + 2 + dataLength;
        }
        metadata_require(offset == bytes.size() - 2, "Saved entry boundaries do not reach checksum");
        auto reloaded = tivars::TIVarFile::loadFromFile(path);
        metadata_require(!reloaded.isCorrupt(), "Saved metadata file reloads as corrupt");
        metadata_require(reloaded.getVarEntries().size() == entryCount, "Saved entry count changed");
        for (const auto& entry : reloaded.getVarEntries())
            metadata_require(entry.data == data_t({0, 0x80, 0x10, 0, 0, 0, 0, 0, 0}),
                             "Saving metadata changed the real-number payload");
    }

    inline void test_activitygrapher_metadata()
    {
        const std::string input = "/tmp/tivars_activitygrapher_metadata_input.8xn";
        const std::string output = "/tmp/tivars_activitygrapher_metadata_output.8xn";
        metadata_write(input, metadata_fixture({11}));
        auto shortFile = tivars::TIVarFile::loadFromFile(input);
        shortFile.saveVarToFile(output);
        metadata_check_saved(output, 13, 1);
        metadata_require(metadata_read(output).size() == 83, "Normalized metadata size is unexpected");

        metadata_write(input, metadata_fixture({13}));
        auto normalFile = tivars::TIVarFile::loadFromFile(input);
        normalFile.saveVarToFile(output);
        metadata_check_saved(output, 13, 1);
        metadata_require(metadata_read(output) == metadata_read(input), "Normal 13-byte entry changed on save");

        metadata_write(input, metadata_fixture({11, 13}));
        auto mixedFile = tivars::TIVarFile::loadFromFile(input);
        mixedFile.saveVarToFile(output);
        metadata_check_saved(output, 13, 2);

        metadata_write(input, metadata_fixture({11}));
        auto archivedFile = tivars::TIVarFile::loadFromFile(input);
        archivedFile.setArchived(true);
        archivedFile.saveVarToFile(output);
        metadata_check_saved(output, 13, 1);
        auto archivedReload = tivars::TIVarFile::loadFromFile(output);
        metadata_require(archivedReload.getVarEntries()[0].archivedFlag == 0x80,
                         "Normalizing 11-byte metadata lost the archive flag");

        std::remove(input.c_str());
        std::remove(output.c_str());
    }
}
#endif
