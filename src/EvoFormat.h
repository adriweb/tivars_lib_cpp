/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARS_LIB_CPP_EVOFORMAT_H
#define TIVARS_LIB_CPP_EVOFORMAT_H

#include "EvoTypes.h"
#include "TIVarFile.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tivars::EvoFormat
{
    struct EvoPythonScriptInfo
    {
        data_t magic;
        uint32_t bodyLen = 0;
        uint32_t nameLen = 0;
        std::string name;
        uint16_t scriptLen = 0;
        uint8_t scriptType = 0;
        data_t body;
        data_t trailer;
        std::string code;
        bool bodyIsText = false;
    };

    struct EvoCBORValue
    {
        enum class Kind
        {
            Unsigned,
            Bytes,
            Text,
            Array,
            Map,
            Simple,
        };

        Kind kind = Kind::Simple;
        uint64_t unsignedValue = 0;
        data_t bytes;
        std::string text;
        std::vector<EvoCBORValue> array;
        std::map<std::string, EvoCBORValue> map;
        data_t raw;
    };

    uint16_t evo_checksum(const data_t& body);
    bool is_evo_file_data(const data_t& fileData);
    EvoCBORValue parse_cbor_value(const data_t& data, size_t& offset);
    void append_cbor_text(data_t& out, std::string_view text);
    void append_cbor_bytes(data_t& out, const data_t& bytes);
    void append_cbor_key_uint(data_t& out, std::string_view key, uint64_t value);

    std::string decode_evo_name(EvoTypeID evoTypeID, const data_t& nameBytes);
    data_t encode_evo_name(EvoTypeID evoTypeID, std::string displayName);
    TIVarType type_from_evo_type(EvoTypeID evoTypeID);
    EvoTypeID evo_type_from_type(const TIVarType& type);
    std::string bytes_to_hex_string(const data_t& data);
    EvoPythonScriptInfo parse_evo_python_script_payload(const data_t& data);

    std::string detokenize_evo_token_words(const data_t& data);
    data_t tokenize_evo_token_words(const std::string& source, const options_t& options = options_t());
    bool is_evo_tokenized_entry(const TIVarFile::var_entry_t& entry);
    data_t evo_tokenized_data_to_legacy(const data_t& evoData);
    data_t legacy_tokenized_data_to_evo(const data_t& legacyData, bool smartConversion = false);

    data_t evo_scalar_to_legacy_value(const data_t& evoData, size_t& offset);
    data_t legacy_value_to_evo_expression(const data_t& legacyValue);
    bool legacy_value_is_exact_fraction(const data_t& legacyValue);
    data_t legacy_list_to_evo(const data_t& legacyData, bool complexList, std::map<std::string, uint64_t>& fields);
    data_t evo_list_to_legacy(const data_t& evoData, uint64_t len, bool& complexList);
    data_t legacy_matrix_to_evo(const data_t& legacyData, std::map<std::string, uint64_t>& fields);
    data_t evo_matrix_to_legacy(const data_t& evoData, uint64_t rows, uint64_t cols);
    std::string evo_matrix_to_readable(const data_t& evoData, uint64_t rows, uint64_t cols, const options_t& options = options_t());
    data_t legacy_picture_to_evo(const data_t& legacyData, std::map<std::string, uint64_t>& fields);
    data_t evo_picture_to_legacy(const data_t& evoData);
    data_t legacy_image_to_evo(const data_t& legacyData, std::map<std::string, uint64_t>& fields);
    data_t evo_image_to_legacy(const data_t& evoData);

    bool is_legacy_numeric_entry(const TIVarFile::var_entry_t& entry);
    void set_entry_type(TIVarFile::var_entry_t& entry, const TIVarType& type);
    void set_numeric_entry_type_from_payload(TIVarFile::var_entry_t& entry);
}

#endif //TIVARS_LIB_CPP_EVOFORMAT_H
