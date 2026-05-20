/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARS_LIB_CPP_EVOTYPES_H
#define TIVARS_LIB_CPP_EVOTYPES_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tivars::EvoFormat
{
    enum class EvoTypeID : uint8_t
    {
        Real = 0,
        List = 1,
        Program = 2,
        GraphDataBase = 3,
        Picture = 4,
        Image = 5,
        Matrix = 6,
        Equation = 7,
        AppVar = 8,
        GroupObject = 9,
        String = 10,
        FlashApp = 11,
        WindowSettings = 12,
        RecallWindow = 13,
        TableRange = 14,
        PythonScript = 15,
    };

    constexpr uint8_t evo_type_id_value(EvoTypeID type)
    {
        return static_cast<uint8_t>(type);
    }

    constexpr EvoTypeID evo_type_id_from_value(uint8_t value)
    {
        return static_cast<EvoTypeID>(value);
    }

    struct EvoTypeInfo
    {
        std::string_view legacyTypeName;
        std::string_view typeName;
        std::string_view extension;
        std::array<std::string_view, 9> legacyTypeAliases;
    };

    inline constexpr std::array<EvoTypeInfo, evo_type_id_value(EvoTypeID::PythonScript) + 1> evoTypeInfos = {{
        {"Real",           "Real",           "8xn2",  {"Complex", "RealFraction", "ExactComplexFrac", "ExactRealRadical", "ExactComplexRadical", "ExactComplexPi", "ExactComplexPiFrac", "ExactRealPi", "ExactRealPiFrac"}},
        {"RealList",       "List",           "8xl2",  {"ComplexList"}},
        {"Program",        "Program",        "8xp2",  {"ProtectedProgram"}},
        {"GraphDataBase",  "GraphDataBase",  "8xd2",  {}},
        {"Picture",        "Picture",        "8ci2",  {}},
        {"Image",          "Image",          "8ca2",  {}},
        {"Matrix",         "Matrix",         "8xm2",  {}},
        {"Equation",       "Equation",       "8xy2",  {"SmartEquation"}},
        {"AppVar",         "AppVar",         "8xv2",  {}},
        {"GroupObject",    "GroupObject",    "8xg2",  {}},
        {"String",         "String",         "8xs2",  {}},
        {"FlashApp",       "FlashApp",       "8ek2",  {}},
        {"WindowSettings", "WindowSettings", "8xw2",  {}},
        {"RecallWindow",   "RecallWindow",   "8xz2",  {}},
        {"TableRange",     "TableRange",     "8xt2",  {}},
        {"PythonAppVar",   "PythonScript",   "8xpy2", {}},
    }};

    inline const EvoTypeInfo& evo_type_info(EvoTypeID evoTypeID)
    {
        const uint8_t value = evo_type_id_value(evoTypeID);
        if (value >= evoTypeInfos.size())
        {
            throw std::invalid_argument("Unknown Evo type ID " + std::to_string(value));
        }
        return evoTypeInfos[value];
    }

    inline std::string_view ti_type_name_from_evo_type(EvoTypeID evoTypeID)
    {
        return evo_type_info(evoTypeID).legacyTypeName;
    }

    inline std::string type_name_from_evo_type(EvoTypeID evoTypeID)
    {
        return std::string{evo_type_info(evoTypeID).typeName};
    }

    inline std::string extension_from_evo_type(EvoTypeID evoTypeID)
    {
        return std::string{evo_type_info(evoTypeID).extension};
    }

    inline bool evo_type_info_matches_ti_type_name(const EvoTypeInfo& info, std::string_view typeName)
    {
        if (typeName == info.legacyTypeName)
        {
            return true;
        }
        for (const std::string_view alias : info.legacyTypeAliases)
        {
            if (!alias.empty() && typeName == alias)
            {
                return true;
            }
        }
        return false;
    }

    inline std::string extension_from_ti_type_name(std::string_view typeName)
    {
        for (const EvoTypeInfo& info : evoTypeInfos)
        {
            if (evo_type_info_matches_ti_type_name(info, typeName))
            {
                return std::string{info.extension};
            }
        }
        return "";
    }

    inline EvoTypeID evo_type_id_from_ti_type_name(std::string_view typeName)
    {
        for (uint8_t value = 0; value < evoTypeInfos.size(); ++value)
        {
            if (evo_type_info_matches_ti_type_name(evoTypeInfos[value], typeName))
            {
                return evo_type_id_from_value(value);
            }
        }
        throw std::invalid_argument("No Evo type mapping for TI type " + std::string{typeName});
    }
}

#endif //TIVARS_LIB_CPP_EVOTYPES_H
