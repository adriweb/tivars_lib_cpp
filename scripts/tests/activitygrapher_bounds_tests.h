/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * License: MIT
 */
#ifndef TIVARS_ACTIVITYGRAPHER_BOUNDS_TESTS_H
#define TIVARS_ACTIVITYGRAPHER_BOUNDS_TESTS_H

#include "../../src/TIVarFile.h"
#include "../../src/TypeHandlers/TypeHandlers.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace activitygrapher_tests
{
    namespace bounds_details
    {
        inline void require(bool condition, const char* message)
        {
            if (!condition) throw std::runtime_error(message);
        }
        template<class Function> inline void reject_length(Function action)
        {
            bool rejected = false;
            try { action(); } catch (const std::length_error&) { rejected = true; }
            require(rejected, "Oversized legacy variable file was not rejected");
        }
        inline void append16(data_t& bytes, uint16_t value)
        {
            bytes.push_back(static_cast<uint8_t>(value));
            bytes.push_back(static_cast<uint8_t>(value >> 8));
        }
        inline data_t read(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);
            require(file.good(), "Cannot read bounds fixture");
            return data_t(std::istreambuf_iterator<char>(file), {});
        }
        inline void write(const std::string& path, const data_t& bytes)
        {
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            require(file.good(), "Cannot write bounds fixture");
        }
        inline data_t group_fixture(size_t payloadLength)
        {
            data_t entries;
            for (uint8_t name : {'A', 'B'})
            {
                append16(entries, 13);
                append16(entries, static_cast<uint16_t>(payloadLength));
                entries.push_back(0x15); // AppVar.
                entries.push_back(name);
                entries.insert(entries.end(), 9, 0); // Remaining name, version and archive flag.
                append16(entries, static_cast<uint16_t>(payloadLength));
                entries.insert(entries.end(), payloadLength, 0);
            }
            data_t bytes = {'*', '*', 'T', 'I', '8', '3', 'F', '*', 0x1A, 0x0A, 0};
            bytes.insert(bytes.end(), 42, 0);
            append16(bytes, static_cast<uint16_t>(entries.size()));
            bytes.insert(bytes.end(), entries.begin(), entries.end());
            uint16_t sum = 0;
            for (uint8_t byte : entries) sum += byte;
            append16(bytes, sum);
            return bytes;
        }
    }

    inline void test_activitygrapher_bounds()
    {
        using namespace tivars;
        using namespace bounds_details;
        const std::string input = "/tmp/tivars_activitygrapher_bounds_input.8xg";
        const std::string output = "/tmp/tivars_activitygrapher_bounds_output.8xv";
        auto app = TIVarFile::createNew(TIVarType{"AppVar"}, "BOUND", TIModel{"84+"});
        app.setContentFromData({0, 0});
        const auto initialData = app.getRawContent();
        const auto initialHeaderLength = app.getHeader().entries_len;
        reject_length([&] { app.setContentFromData(data_t(65536, 0)); });
        require(app.getRawContent() == initialData, "Rejected raw payload changed object data");
        require(app.getHeader().entries_len == initialHeaderLength, "Rejected raw payload changed metadata");

        // A 13-byte entry has 17 bytes of framing; its payload can reach 65518.
        app.setContentFromData(data_t(65518, 0));
        app.saveVarToFile(output);
        require(read(output).size() == 65592, "Maximum legacy entry saved with an unexpected size");
        auto largest = TIVarFile::loadFromFile(output);
        require(!largest.isCorrupt() && largest.getRawContent().size() == 65518,
                "Maximum legacy entry does not reload correctly");
        const auto largestBytes = read(output);
        reject_length([&] { app.setContentFromData(data_t(65519, 0)); });
        app.saveVarToFile(output);
        require(read(output) == largestBytes, "Aggregate overflow changed the previous valid entry");

        auto matrix = TIVarFile::createNew(TIVarType{"Matrix"}, "", TIModel{"84+CE"});
        matrix.setContentFromString("[[7]]");
        const auto originalMatrix = matrix.getRawContent();
        std::string text = "[";
        for (size_t row = 0; row < 86; row++)
        {
            text += '[';
            for (size_t col = 0; col < 86; col++) text += col ? ",0" : "0";
            text += ']';
        }
        text += ']';
        reject_length([&] { matrix.setContentFromString(text); });
        require(matrix.getRawContent() == originalMatrix, "Rejected matrix text changed object data");

        write(input, group_fixture(2));
        auto group = TIVarFile::loadFromFile(input);
        group.setContentFromData(data_t(32760, 0), 0);
        const auto validGroupLength = group.getHeader().entries_len;
        reject_length([&] { group.setContentFromData(data_t(32760, 0), 1); });
        require(group.getRawContent(0).size() == 32760 && group.getRawContent(1).size() == 2,
                "Rejected group replacement changed an entry");
        require(group.getHeader().entries_len == validGroupLength, "Rejected group replacement changed metadata");

        // The existing loader accepts a wrapped aggregate header. Saving must reject it
        // before opening/truncating an existing destination.
        write(input, group_fixture(32751)); // Entry region would be 65536 bytes.
        auto tooLargeGroup = TIVarFile::loadFromFile(input);
        const data_t sentinel = {'S', 'A', 'F', 'E'};
        write(output, sentinel);
        reject_length([&] { tooLargeGroup.saveVarToFile(output); });
        require(read(output) == sentinel, "Rejected save truncated the previous output file");

        TypeHandlers::TH_Backup::backup_contents_t contents{};
        contents.data1 = {1}; contents.data2 = {2}; contents.data3 = {3};
        auto backup = TIVarFile::createNew(TIVarType{"Backup"}, "BACKUP", TIModel{"84+"});
        backup.setContentFromData(TypeHandlers::TH_Backup::buildInternal(contents));
        const auto originalBackup = backup.getRawContent();
        contents.data1.assign(22000, 0); contents.data2.assign(22000, 0); contents.data3.assign(22000, 0);
        reject_length([&] { backup.setContentFromData(TypeHandlers::TH_Backup::buildInternal(contents)); });
        require(backup.getRawContent() == originalBackup, "Rejected backup overflow changed object data");
        std::remove(input.c_str());
        std::remove(output.c_str());
    }
}
#endif
