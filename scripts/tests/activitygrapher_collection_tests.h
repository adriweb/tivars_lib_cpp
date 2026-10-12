#ifndef ACTIVITYGRAPHER_COLLECTION_TESTS_H
#define ACTIVITYGRAPHER_COLLECTION_TESTS_H

#include "../../src/TIVarFile.h"
#include "../../src/EvoFormat.h"
#include "../../src/TypeHandlers/TypeHandlers.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace activitygrapher_tests
{
    inline void require_collection(bool condition, const char* description)
    {
        if (!condition) throw std::runtime_error(description);
    }

    inline void test_activitygrapher_collections()
    {
        using namespace tivars;
        using namespace tivars::TypeHandlers;

        // Bit 5 belongs to the six-bit numeric subtype, including complex members.
        // CEmu / CE OS 5.6.1 rejects 0x2C in the first member of a complex value.
        const data_t ordinaryComplex{0x0C,0x80,0x10,0,0,0,0,0,0,0x0C,0x80,0x20,0,0,0,0,0,0};
        for (uint8_t realFlags : {0x00, 0x40, 0x80, 0xC0})
        {
            for (uint8_t imagFlags : {0x00, 0x40, 0x80, 0xC0})
            {
                auto member = ordinaryComplex;
                member[0] |= realFlags;
                member[9] |= imagFlags;
                const std::string expected = std::string(realFlags & 0x80 ? "-1" : "1")
                    + (imagFlags & 0x80 ? "-2i" : "+2i");
                require_collection(TH_GenericComplex::makeStringFromData(member) == expected,
                                   "Complex decoding must retain signs and ignore the graph flag");
                require_collection(TH_GenericComplex::getMinVersionFromData(member) == VER_NONE,
                                   "Sign and graph flags must not raise the complex version");
            }
        }

        const std::vector<std::pair<uint8_t, std::string>> complexCases{
            {0x0C, "1+2i"}, {0x1B, "1+1/2i"}, {0x1D, "1+(√(2)+0*√(1))/1i"},
            {0x1E, "1+2πi"}, {0x1F, "1+π/2i"},
        };
        for (const auto& [subtype, text] : complexCases)
        {
            const auto canonical = TH_GenericComplex::makeDataFromString(text, {{"_type", subtype}});
            require_collection(!TH_GenericComplex::makeStringFromData(canonical).empty(),
                               "Canonical complex subtypes must still decode");
            const auto version = subtype == 0x0C ? VER_NONE : subtype == 0x1B ? VER_CE_ALL : VER_CE_EXACTONLY;
            require_collection(TH_GenericComplex::getMinVersionFromData(canonical) == version,
                               "Canonical complex subtypes must keep their required versions");
            for (size_t offset : {size_t(0), size_t(9)})
            {
                auto invalid = canonical;
                invalid[offset] |= 0x20;
                bool rejected = false;
                try { (void)TH_GenericComplex::makeStringFromData(invalid); }
                catch (const std::runtime_error&) { rejected = true; }
                require_collection(rejected, "Bit 5 must not alias an unknown subtype to a valid complex member");
                require_collection(TH_GenericComplex::getMinVersionFromData(invalid) == VER_INVALID,
                                   "Unknown complex subtypes must not receive a compatible version");
                data_t invalidList{1, 0};
                invalidList.insert(invalidList.end(), invalid.begin(), invalid.end());
                rejected = false;
                try { (void)TH_GenericList::makeStringFromData(invalidList); }
                catch (const std::runtime_error&) { rejected = true; }
                require_collection(rejected, "Complex-list decoding must reject unknown member subtypes too");
            }
        }

        for (const size_t offset : {size_t(0), size_t(9)})
        {
            data_t invalidList{1, 0};
            invalidList.insert(invalidList.end(), ordinaryComplex.begin(), ordinaryComplex.end());
            invalidList[offset + 2] |= 0x20;
            auto repaired = TIVarFile::createNew("ComplexList", "", TIModel{"83PCE"});
            repaired.setContentFromData(invalidList);
            repaired.setContentFromString("{1+2i}");
            data_t expected{1, 0};
            expected.insert(expected.end(), ordinaryComplex.begin(), ordinaryComplex.end());
            require_collection(repaired.getRawContent() == expected,
                               "Collection edits must replace unknown complex subtypes instead of preserving them");
        }

        for (const data_t& malformed : {data_t{}, data_t(9), data_t(17), data_t(19)})
            require_collection(TH_GenericComplex::getMinVersionFromData(malformed) == VER_INVALID,
                               "Malformed complex payloads must not receive a compatible version");

        for (const auto& value : {TH_GenericReal::makeDataFromString("1/2", {{"_type", 0x18}}),
                                 TH_GenericComplex::makeDataFromString("1/2+i", {{"_type", 0x0C}})})
        {
            for (uint8_t flags : {0x00, 0x40, 0x80, 0xC0})
            {
                auto flagged = value;
                flagged[0] |= flags;
                require_collection(EvoFormat::legacy_value_is_exact_fraction(flagged),
                                   "Evo conversion must recognize fraction types with sign and graph flags");
                flagged[0] |= 0x20;
                require_collection(!EvoFormat::legacy_value_is_exact_fraction(flagged),
                                   "Evo conversion must not alias unknown subtypes to exact fractions");
            }
        }

        for (const std::string file : {"RealList.8xl", "ComplexList.8xl", "Matrix_3x3_standard.8xm", "Matrix_2x2_exact.8xm"})
        {
            auto original = TIVarFile::loadFromFile("testData/" + file);
            const auto raw = original.getRawContent();
            auto fresh = TIVarFile::createNew(original.getVarEntries()[0]._type, "", original.getCalcModel());
            fresh.setContentFromString(original.getReadableContent());
            require_collection(fresh.getRawContent() == raw, "Collection fixture text must rebuild the numeric payload");
            original.setContentFromString(original.getReadableContent());
            require_collection(original.getRawContent() == raw, "Unchanged collection text must preserve payload bytes");
        }

        auto list = TIVarFile::createNew("RealList", "", TIModel{"83PCE"});
        list.setContentFromString("{1/2,3π,π/2,(√(2)+0*√(1))/1,4}");
        auto raw = list.getRawContent();
        const std::vector<int> expectedTypes{0x18, 0x20, 0x21, 0x1C, 0x00};
        for (size_t i = 0; i < expectedTypes.size(); ++i)
            require_collection((raw[2 + i * 9] & 0x3F) == expectedTypes[i], "Real-list member must select its exact handler");
        list.setContentFromString(list.getReadableContent());
        require_collection(list.getRawContent() == raw, "Mixed exact list must round-trip");

        auto complexList = TIVarFile::createNew("ComplexList", "", TIModel{"83PCE"});
        complexList.setContentFromString("{1+1/2i,3π+2i,1+(√(2)+0*√(1))/1i,1+π/2i}");
        raw = complexList.getRawContent();
        require_collection((raw[2 + 9] & 0x3F) == 0x1B, "Complex list must infer imaginary fraction type");
        require_collection((raw[2 + 18] & 0x3F) == 0x1E, "Complex list must infer real pi type");
        require_collection((raw[2 + 36 + 9] & 0x3F) == 0x1D, "Complex list must infer imaginary radical type");
        require_collection((raw[2 + 54 + 9] & 0x3F) == 0x1F, "Complex list must infer imaginary pi fraction type");
        complexList.setContentFromString(complexList.getReadableContent());
        require_collection(complexList.getRawContent() == raw, "Mixed exact complex list must round-trip");

        for (const std::string type : {"RealList", "Matrix"})
        {
            auto fallback = TIVarFile::createNew(type, "", TIModel{"83PCE"});
            fallback.setContentFromString(type == "Matrix" ? "[[0.33333333333334π]]" : "{0.33333333333334π}");
            const auto expected = TH_GenericReal::makeDataFromString("0.33333333333334π", {{"_type", 0x21}});
            const auto actual = fallback.getRawContent();
            require_collection(data_t(actual.begin() + 2, actual.end()) == expected,
                               "Decimal pi coefficients must rebuild exact fraction digits");
        }

        for (const std::string type : {"RealList", "Matrix", "ComplexList"})
        {
            const bool complex = type == "ComplexList";
            const data_t member = complex
                ? TH_GenericComplex::makeDataFromString("1+2i", {{"_type", 0x1B}})
                : TH_GenericReal::makeDataFromString("2", {{"_type", 0x18}});
            data_t data{1, uint8_t(type == "Matrix" ? 1 : 0)};
            data.insert(data.end(), member.begin(), member.end());
            auto value = TIVarFile::createNew(type, "", TIModel{"83PCE"});
            value.setContentFromData(data);
            value.setContentFromString(value.getReadableContent());
            require_collection(value.getRawContent() == data, "Integer text must preserve existing exact member subtype");
        }

        // Changing a neighboring element must not downgrade an unchanged exact integer.
        list.setContentFromData({2,0,0x18,0x80,0x20,0,0,0,0,0,0,0,0x80,0x30,0,0,0,0,0,0});
        list.setContentFromString("{2,4}");
        require_collection(list.getRawContent()[2] == 0x18, "Collection edits must preserve unchanged exact integer members");

        for (const std::string type : {"RealList", "Matrix"})
        {
            auto repair = TIVarFile::createNew(type, "", TIModel{"83PCE"});
            data_t unsupported{1, uint8_t(type == "Matrix" ? 1 : 0), 0x19,0x80,0x20,0,0,0,0,0,0};
            repair.setContentFromData(unsupported);
            repair.setContentFromString(type == "Matrix" ? "[[3]]" : "{3}");
            require_collection(repair.getRawContent()[2] == 0x00, "Valid replacement must repair an unsupported old subtype");
        }

        // A grouped file must use the selected entry as the preservation source.
        const std::string groupPath = "/tmp/tivars_activitygrapher_collection_group.8xg";
        data_t entries;
        for (uint8_t subtype : {uint8_t(0x00), uint8_t(0x18)})
        {
            data_t entry{13,0,11,0,1,0x5D,0,0,0,0,0,0,0,0,0,11,0,1,0,subtype,0x80,0x20,0,0,0,0,0,0};
            entry[6] = static_cast<uint8_t>(entries.empty() ? 0 : 1);
            entries.insert(entries.end(), entry.begin(), entry.end());
        }
        data_t bytes{'*','*','T','I','8','3','F','*',0x1A,0x0A,0x13};
        bytes.insert(bytes.end(), 42, 0);
        bytes.push_back(static_cast<uint8_t>(entries.size()));
        bytes.push_back(0);
        bytes.insert(bytes.end(), entries.begin(), entries.end());
        uint16_t checksum = 0;
        for (uint8_t value : entries) checksum += value;
        bytes.push_back(static_cast<uint8_t>(checksum));
        bytes.push_back(static_cast<uint8_t>(checksum >> 8));
        {
            std::ofstream file(groupPath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            require_collection(file.good(), "Unable to write grouped collection fixture");
        }
        auto group = TIVarFile::loadFromFile(groupPath);
        const auto second = group.getRawContent(1);
        group.setContentFromString(group.getReadableContent({}, 1), {}, 1);
        require_collection(group.getRawContent(1) == second, "Grouped edit must preserve the selected entry subtype");
        std::remove(groupPath.c_str());

        for (const std::string text : {"[[1+2i]]", "[[1,2][3]]", "[[1x/2]]", "[[3π*√2]]"})
        {
            bool rejected = false;
            try { (void)TH_Matrix::makeDataFromString(text); }
            catch (const std::exception&) { rejected = true; }
            require_collection(rejected, "Matrix must reject complex, ragged, or malformed members");
        }
    }
}
#endif
