#include <atomic>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

#include "../src/TIFlashFile.h"
#include "../src/TIModels.h"
#include "../src/TIVarFile.h"
#include "../src/TIVarTypes.h"
#include "../src/TypeHandlers/TypeHandlers.h"
#include "../src/tivarslib_utils.h"

#ifndef TIVARS_PROGRAMS_TOKENS_CSV
#define TIVARS_PROGRAMS_TOKENS_CSV "programs_tokens.csv"
#endif

namespace
{
    constexpr size_t maxInputSize = 2U * 1024U * 1024U;
    constexpr size_t maxRoundtripTextSize = 128U * 1024U;

    std::once_flag initFlag;
    std::atomic<uint64_t> inputCounter{0};

    void init_library()
    {
        std::call_once(initFlag, []()
        {
            std::setlocale(LC_ALL, ".UTF-8");
            tivars::TIModels::initTIModelsArray();
            tivars::TIVarTypes::initTIVarTypesArray();
            tivars::TypeHandlers::TH_Tokenized::initTokensFromCSVFilePath(TIVARS_PROGRAMS_TOKENS_CSV);
        });
    }

    std::filesystem::path write_input_to_temp_file(const uint8_t* data, size_t size)
    {
        std::error_code ec;
        std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
        if (ec)
        {
            tempDir = std::filesystem::current_path();
        }

        const uint64_t uniqueId = inputCounter.fetch_add(1, std::memory_order_relaxed);
        const std::filesystem::path path = tempDir / ("tivars-fuzz-" + std::to_string(uniqueId) + ".bin");
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        out.close();
        if (!out)
        {
            throw std::runtime_error("Failed to write fuzz input to a temp file");
        }
        return path;
    }

    void exercise_var_file(const std::filesystem::path& path)
    {
        tivars::TIVarFile file = tivars::TIVarFile::loadFromFile(path.string());
        (void)file.getInstanceChecksum();
        (void)file.isCorrupt();

        const auto& entries = file.getVarEntries();
        for (size_t i = 0; i < entries.size(); i++)
        {
            const uint16_t entryIndex = static_cast<uint16_t>(i);
            (void)file.getRawContent(entryIndex).size();
            const std::string readable = file.getReadableContent({{"prettify", 1}, {"reindent", 1}}, entryIndex);

            if (entries.size() == 1 && readable.size() <= maxRoundtripTextSize)
            {
                const std::string name = tivars::entry_name_to_string(entries[0]._type, entries[0].varname, sizeof(entries[0].varname));
                tivars::TIVarFile roundtrip = tivars::TIVarFile::createNew(entries[0]._type, name, file.getCalcModel());
                roundtrip.setContentFromString(readable);
                (void)roundtrip.getRawContent().size();
            }
        }
    }

    void exercise_flash_file(const std::filesystem::path& path)
    {
        tivars::TIFlashFile file = tivars::TIFlashFile::loadFromFile(path.string());
        const auto& headers = file.getHeaders();
        for (size_t i = 0; i < headers.size(); i++)
        {
            (void)file.getReadableContent(i).size();
        }
        (void)file.makeBinData().size();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    init_library();

    if (size == 0 || size > maxInputSize)
    {
        return 0;
    }

    std::filesystem::path path;
    try
    {
        path = write_input_to_temp_file(data, size);

        try
        {
            exercise_var_file(path);
        }
        catch (...)
        {
        }

        try
        {
            exercise_flash_file(path);
        }
        catch (...)
        {
        }
    }
    catch (...)
    {
    }

    if (!path.empty())
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    return 0;
}
