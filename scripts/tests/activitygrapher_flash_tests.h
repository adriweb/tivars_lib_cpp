/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARS_ACTIVITYGRAPHER_FLASH_TESTS_H
#define TIVARS_ACTIVITYGRAPHER_FLASH_TESTS_H

#include "../../src/TIFlashFile.h"
#include "../../src/json.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace activitygrapher_tests
{
    namespace flash_details
    {
        using namespace tivars;
        using json = nlohmann::ordered_json;

        struct Checks
        {
            size_t count = 0;

            void require(bool condition, const char* message)
            {
                count++;
                if (!condition)
                {
                    throw std::runtime_error(std::string("Flash container regression: ") + message);
                }
            }
        };

        inline data_t read_bytes(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Cannot read flash regression fixture");
            }
            return data_t(std::istreambuf_iterator<char>(file), {});
        }

        inline void write_bytes(const std::string& path, const data_t& bytes)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!file)
            {
                throw std::runtime_error("Cannot write flash regression fixture");
            }
        }

        inline std::string hex_bytes(const std::string& bytes)
        {
            const char* digits = "0123456789ABCDEF";
            std::string hex;
            for (const unsigned char byte : bytes)
            {
                hex += digits[byte >> 4];
                hex += digits[byte & 0x0F];
            }
            return hex;
        }

        inline TIFlashFile intel_file(const std::string& payload)
        {
            TIFlashFile file = TIFlashFile::createNew(TIVarType{"FlashApp"}, "APP", TIModel{"84+"});
            file.setContentFromString(json{{"rawDataHex", hex_bytes(payload)}}.dump());
            return file;
        }

        template<typename Exception, typename Action>
        inline void expect_error(Checks& checks, Action action, const char* expected)
        {
            bool rejected = false;
            try
            {
                action();
            }
            catch (const Exception& error)
            {
                rejected = std::string(error.what()) == expected;
            }
            checks.require(rejected, expected);
        }
    }

    inline void test_activitygrapher_flash()
    {
        using namespace tivars;
        using namespace flash_details;
        Checks checks;
        const std::string path = "/tmp/tivars_activitygrapher_flash.8xk";
        const std::string eof = ":00000001FF";
        const std::vector<std::string> records = {
            ":020000020000FC",
            ":0440000001020304B2",
            eof
        };
        const json expectedBlocks = json::array({
            {{"address", "0000"}, {"blockType", "02"}, {"rawDataHex", "0000"}},
            {{"address", "4000"}, {"blockType", "00"}, {"rawDataHex", "01020304"}},
            {{"address", "0000"}, {"blockType", "01"}, {"rawDataHex", ""}}
        });

        TIFlashFile fresh = TIFlashFile::createNew(TIVarType{"FlashApp"}, "NEW", TIModel{"84+"});
        checks.require(fresh.getHeaders()[0].binaryFlag == TIFlashFile::intelDataFlag, "new flash Intel flag");
        checks.require(fresh.getHeaders()[0].calcData == data_t(eof.begin(), eof.end()), "new flash ASCII EOF");
        const json freshReadable = json::parse(fresh.getReadableContent());
        checks.require(freshReadable["blocks"] == json::array({expectedBlocks[2]}), "new flash readable EOF");
        fresh.saveToFile(path);
        checks.require(TIFlashFile::loadFromFile(path).makeBinData() == fresh.makeBinData(), "new flash reload");

        fresh.addHeader(TIVarType{"FlashApp"}, "SECOND", TIModel{"84+"});
        checks.require(fresh.getHeaders()[1].calcData == data_t(eof.begin(), eof.end()), "added flash ASCII EOF");
        fresh.saveToFile(path);
        const TIFlashFile twoHeaders = TIFlashFile::loadFromFile(path);
        checks.require(twoHeaders.getHeaders().size() == 2, "two valid headers loaded");
        checks.require(twoHeaders.makeBinData() == fresh.makeBinData(), "two headers exact round trip");

        TIFlashFile raw = TIFlashFile::createNew(TIVarType{"FlashApp"}, "RAW", TIModel{"84+"}, true);
        checks.require(raw.getHeaders()[0].binaryFlag == TIFlashFile::rawBinaryDataFlag, "raw flash flag");
        checks.require(raw.getHeaders()[0].calcData.empty(), "raw flash remains empty");
        raw.saveToFile(path);
        checks.require(TIFlashFile::loadFromFile(path).makeBinData() == raw.makeBinData(), "raw flash reload");

        for (const std::string& newline : {std::string("\r\n"), std::string("\n"), std::string("\r")})
        {
            for (const bool trailingNewline : {false, true})
            {
                std::string payload = records[0] + newline + records[1] + newline + records[2];
                if (trailingNewline)
                {
                    payload += newline;
                }
                TIFlashFile file = intel_file(payload);
                file.saveToFile(path);
                const data_t original = read_bytes(path);
                const TIFlashFile loaded = TIFlashFile::loadFromFile(path);
                checks.require(json::parse(loaded.getReadableContent())["blocks"] == expectedBlocks, "Intel line ending blocks");
                checks.require(loaded.getHeaders()[0].calcData == data_t(payload.begin(), payload.end()), "Intel payload preserved");
                checks.require(loaded.makeBinData() == original, "Intel exact container round trip");

                // Record validation must not weaken when accepting other line endings.
                for (const auto& [record, error] : std::vector<std::pair<std::string, const char*>>{
                    {":04400000010203B2", "Unexpected Intel flash record length"},
                    {":0440000001020304B3", "Invalid Intel flash record checksum"}
                })
                {
                    TIFlashFile invalid = intel_file(records[0] + newline + record + newline + eof);
                    invalid.saveToFile(path);
                    const TIFlashFile invalidLoaded = TIFlashFile::loadFromFile(path);
                    expect_error<std::invalid_argument>(checks, [&] { (void)invalidLoaded.getReadableContent(); }, error);
                }
            }
        }

        TIFlashFile mixed = intel_file("\n\r\n" + records[0] + "\r" + records[1] + "\n\n" + eof + "\r\n");
        checks.require(json::parse(mixed.getReadableContent())["blocks"] == expectedBlocks, "mixed line endings and empty lines");

        // Check both the initial magic and each subsequent header's magic.
        for (const bool checksum : {false, true})
        {
            TIFlashFile single = intel_file(eof);
            single.setContentFromString(json{{"hasChecksum", checksum}}.dump());
            const data_t valid = single.makeBinData();
            write_bytes(path, valid);
            checks.require(TIFlashFile::loadFromFile(path).makeBinData() == valid, "optional outer checksum round trip");
            data_t badMagic = valid;
            badMagic[0] = '!';
            write_bytes(path, badMagic);
            expect_error<std::runtime_error>(checks, [&] { (void)TIFlashFile::loadFromFile(path); }, "Invalid TI flash header magic");

            // The first checksum removes ambiguity about the second header boundary.
            TIFlashFile multi = intel_file(eof);
            const size_t secondOffset = multi.makeBinData().size();
            multi.addHeader(TIVarType{"FlashApp"}, "SECOND", TIModel{"84+"});
            multi.setContentFromString(json{{"hasChecksum", checksum}}.dump(), 1);
            badMagic = multi.makeBinData();
            badMagic[secondOffset] = '!';
            write_bytes(path, badMagic);
            expect_error<std::runtime_error>(checks, [&] { (void)TIFlashFile::loadFromFile(path); }, "Invalid TI flash header magic");
        }

        data_t badOuterChecksum = fresh.makeBinData();
        badOuterChecksum[TIFlashFile::headerByteCountWithoutChecksum + eof.size()] ^= 1;
        write_bytes(path, badOuterChecksum);
        expect_error<std::runtime_error>(checks, [&] { (void)TIFlashFile::loadFromFile(path); }, "Invalid flash header checksum");

        checks.require(std::remove(path.c_str()) == 0, "remove flash test fixture");
        std::cout << "ActivityGrapher flash regression: " << checks.count << " checks passed\n";
    }
}

#endif
