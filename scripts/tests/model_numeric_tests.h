#ifndef TIVARS_MODEL_NUMERIC_TESTS_H
#define TIVARS_MODEL_NUMERIC_TESTS_H

#include "../../src/TIVarFile.h"
#include "../../src/TypeHandlers/TypeHandlers.h"
#include <stdexcept>

namespace tivars::numeric_regression
{
    inline void check_model_numeric_regressions()
    {
        using namespace TypeHandlers;
        const auto require = [](bool condition) {
            if (!condition) throw std::runtime_error("Model-aware numeric regression failed");
        };
        const auto rejects = [&](TIVarFile& file, const std::string& text) {
            const auto before = file.getRawContent();
            bool rejected = false;
            try { file.setContentFromString(text); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected && file.getRawContent() == before);
        };

        for (const auto* model : {"82", "83", "83+", "82A"})
        {
            auto list = TIVarFile::createNew("RealList", "", TIModel{model});
            list.setContentFromString("{2}");
            rejects(list, "{1/3,2pi}");
            rejects(list, "{1/3}");
            auto matrix = TIVarFile::createNew("Matrix", "", TIModel{model});
            matrix.setContentFromString("[[2]]");
            rejects(matrix, "[[1/3]]");
            rejects(matrix, "[[2pi]]");
        }

        for (const auto* model : {"84+", "84+T", "84+CSE", "84+CE", "84+CEPy"})
        {
            for (const auto* type : {"RealList", "Matrix"})
            {
                auto file = TIVarFile::createNew(type, "", TIModel{model});
                file.setContentFromString(std::string(type) == "Matrix" ? "[[1/3]]" : "{1/3}");
                require((file.getRawContent()[2] & 0x3F) == 0x18);
                require(file.getVarEntries()[0].version == VER_84P_253MP);
                rejects(file, std::string(type) == "Matrix" ? "[[2pi]]" : "{2pi}");
                rejects(file, std::string(type) == "Matrix" ? "[[√(2)]]" : "{√(2)}");
            }
        }

        for (const auto* model : {"83PCE", "83PCEEP", "82AEP"})
        {
            for (const auto* type : {"RealList", "Matrix"})
            {
                auto file = TIVarFile::createNew(type, "", TIModel{model});
                file.setContentFromString(std::string(type) == "Matrix" ? "[[1/3,2pi,√(2)]]" : "{1/3,2pi,√(2)}");
                const auto data = file.getRawContent();
                require((data[2] & 0x3F) == 0x18 && (data[11] & 0x3F) == 0x20 && (data[20] & 0x3F) == 0x1C);
                require(file.getVarEntries()[0].version == VER_CE_EXACTONLY);
            }
            auto scalar = TIVarFile::createNew("Complex", "", TIModel{model});
            auto list = TIVarFile::createNew("ComplexList", "", TIModel{model});
            scalar.setContentFromString("1/2+πi");
            list.setContentFromString("{1/2+πi}");
            const auto listData = list.getRawContent();
            require(scalar.getRawContent() == data_t(listData.begin() + 2, listData.end()));
            require(scalar.getReadableContent() == "1/2+πi");
        }

        auto ceComplex = TIVarFile::createNew("Complex", "", TIModel{"84+CE"});
        ceComplex.setContentFromString("1/2+1/3i");
        require((ceComplex.getRawContent()[0] & 0x1F) == 0x1B);
        require((ceComplex.getRawContent()[9] & 0x1F) == 0x1B);
        require(ceComplex.getVarEntries()[0].version == VER_CE_ALL);
        rejects(ceComplex, "1/2+πi");
        auto legacyComplex = TIVarFile::createNew("Complex", "", TIModel{"83+"});
        legacyComplex.setContentFromString("1+2i");
        rejects(legacyComplex, "1/2+2i");
        rejects(legacyComplex, "1+1/2i");

        // Unchanged integer text must not preserve an incompatible old subtype.
        auto converted = TIVarFile::createNew("RealList", "", TIModel{"83PCE"});
        const auto exactInteger = TH_GenericReal::makeDataFromString("2", {{"_type", 0x18}});
        data_t data{1, 0};
        data.insert(data.end(), exactInteger.begin(), exactInteger.end());
        converted.setContentFromData(data);
        converted.setCalcModel(TIModel{"83"});
        converted.setContentFromString("{2}");
        require(converted.getRawContent()[2] == 0x00);
    }
}
#endif
