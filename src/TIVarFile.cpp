/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarFile.h"
#include "tivarslib_utils.h"
#include "TIModels.h"

#include <stdexcept>
#include <numeric>
#include <regex>

namespace tivars
{

    /*** Constructors ***/

    /**
     * Internal constructor, called from loadFromFile and createNew.
     * If filePath empty or not given, it's a programmer error, and it will throw in BinaryFile(filePath) anyway.
     * To create a TIVarFile not from a file, use TIVarFile::createNew
     * @param   string  filePath
     * @throws  \Exception
     */
    TIVarFile::TIVarFile(const std::string& filePath) : BinaryFile(filePath)
    {
        this->fromFile = true;
        if (this->fileSize < 76) // bare minimum for header + a var entry
        {
            throw std::runtime_error("This file is not a valid TI-[e]z80 variable file");
        }
        this->makeHeaderFromFile();
        this->makeVarEntriesFromFile();
        this->computedChecksum = this->computeChecksumFromFileData();
        this->fileChecksum = this->getChecksumValueFromFile();
        if (this->computedChecksum != this->fileChecksum)
        {
            // puts("[Warning] File is corrupt (read and calculated checksums differ)");
            this->corrupt = true;
        }
        this->close(); // let's free the resource up as early as possible
    }

    TIVarFile TIVarFile::loadFromFile(const std::string& filePath)
    {
        if (!filePath.empty())
        {
            return TIVarFile(filePath);
        } else {
            throw std::runtime_error("No file path given");
        }
    }

    TIVarFile::TIVarFile(const TIVarType& type, const std::string& name, const TIModel& model) : calcModel(model)
    {
        if (!this->calcModel.supportsType(type))
        {
            throw std::runtime_error("This calculator model (" + this->calcModel.getName() + ") does not support the type " + type.getName());
        }

        const std::string signature = this->calcModel.getSig();
        const std::string comment = str_pad("Created by tivars_lib_cpp", sizeof(var_header_t::comment), "\0");

        std::copy(signature.begin(), signature.end(), this->header.signature);
        std::copy(comment.begin(), comment.end(), this->header.comment);
        this->header.entries_len = 0; // will have to be overwritten later

        this->entries.resize(1);
        auto& entry = this->entries[0];
        entry.meta_length  = (this->calcModel.getFlags() & TIFeatureFlags::hasFlash) ? varEntryNewLength : varEntryOldLength;
        entry.data_length  = 0; // will have to be overwritten later
        entry.typeID       = (uint8_t) type.getId();
        entry._type = type;
        entry.setVarName(name);
        entry.data_length2 = 0; // will have to be overwritten later
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const std::string& name, const TIModel& model)
    {
        return TIVarFile(type, name, model);
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const std::string& name)
    {
        return createNew(type, name, "84+");
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type)
    {
        return createNew(type, "");
    }

    /*** Makers ***/

    void TIVarFile::makeHeaderFromFile()
    {
        rewind(this->file);

        const auto signature = this->get_string_bytes(sizeof(var_header_t::signature));
        const auto sig_extra = this->get_raw_bytes(sizeof(var_header_t::sig_extra));
        const auto comment = this->get_string_bytes(sizeof(var_header_t::comment));
        std::copy(signature.begin(), signature.end(), this->header.signature);
        std::copy(sig_extra.begin(), sig_extra.end(), this->header.sig_extra);
        std::copy(comment.begin(), comment.end(), this->header.comment);
        this->header.entries_len  = this->get_two_bytes_swapped();
        this->calcModel = TIModel::createFromSignature(signature); // TODO: check sig_extra bytes instead/too since it has the PID which may be more precise
    }

    void TIVarFile::makeVarEntriesFromFile()
    {
        fseek(this->file, TIVarFile::firstVarEntryOffset, SEEK_SET);

        while (ftell(this->file) < (long)(this->fileSize - 2))
        {
            var_entry_t entry{};
            entry.meta_length = this->get_two_bytes_swapped();
            entry.data_length = this->get_two_bytes_swapped();
            entry.typeID      = this->get_raw_byte();
            const std::string varNameFromFile = this->get_string_bytes(sizeof(var_entry_t::varname));
            if (entry.meta_length == varEntryNewLength)
            {
                if ((this->calcModel.getFlags() & TIFeatureFlags::hasFlash) == 0)
                {
                    fprintf(stderr, "Something is wrong with your file... The var entry meta length indicates is has flash-related fields, but the signature doesn't match...\n");
                }
                entry.version      = this->get_raw_byte();
                entry.archivedFlag = this->get_raw_byte();
            }
            else if (entry.meta_length != varEntryOldLength)
            {
                throw std::invalid_argument("Invalid file. The var entry meta length has an unexpected value. Don't know what to do with that file.");
            }
            entry.data_length2 = this->get_two_bytes_swapped();
            entry.data         = this->get_raw_bytes(entry.data_length);

            // Now that we have the data, we can retrieve the (full) type...
            entry.determineFullType();

            // ... and now that we have the full type, we can safely set the name.
            // TODO: when setVarName knows about all the possible names from types: entry.setVarName(varNameFromFile);
            std::copy(varNameFromFile.begin(), varNameFromFile.end(), entry.varname);

            this->entries.push_back(entry);
        }
    }


    /*** Private actions ***/

    uint16_t TIVarFile::computeChecksumFromFileData()
    {
        if (this->fromFile)
        {
            fseek(this->file, TIVarFile::firstVarEntryOffset, SEEK_SET);

            uint16_t sum = 0;
            for (size_t i = firstVarEntryOffset; i < this->fileSize - 2; i++)
            {
                sum += this->get_raw_byte();
            }
            return (uint16_t) (sum & 0xFFFF);
        } else {
            throw std::runtime_error("[Error] No file loaded");
        }
    }

    uint16_t TIVarFile::computeChecksumFromInstanceData()
    {
        uint16_t sum = 0;
        for (const auto& entry : this->entries)
        {
            sum += entry.meta_length;
            sum += entry.typeID;
            sum += 2 * ((entry.data_length & 0xFF) + ((entry.data_length >> 8) & 0xFF)); // 2* because of the two same length fields
            sum += std::accumulate(entry.varname, entry.varname + sizeof(var_entry_t::varname), 0);
            sum += std::accumulate(entry.data.begin(), entry.data.end(), 0);
            if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
            {
                sum += entry.version;
                sum += entry.archivedFlag;
            }
        }
        return (uint16_t) (sum & 0xFFFF);
    }

    uint16_t TIVarFile::getChecksumValueFromFile()
    {
        if (this->fromFile)
        {
            fseek(this->file, this->fileSize - 2, SEEK_SET);
            return this->get_two_bytes_swapped();
        } else {
            throw std::runtime_error("[Error] No file loaded");
        }
    }

    /**
     *  Updates the length fields in both the header and the var entries, as well as the checksum
     */
    void TIVarFile::refreshMetadataFields()
    {
        this->header.entries_len = 0;
        for (auto& entry : this->entries)
        {
            entry.data_length2 = entry.data_length = (uint16_t) entry.data.size();
            this->header.entries_len += sizeof(var_entry_t::data_length) + sizeof(var_entry_t::data_length2) + entry.meta_length + entry.data_length;
        }
        this->computedChecksum = this->computeChecksumFromInstanceData();
    }

    void TIVarFile::var_entry_t::setVarName(const std::string &name) {
        std::string newName(name);
        if (newName.empty())
        {
            newName = "FILE" + (_type.getExts().empty() ? "" : _type.getExts()[0]);
        }

        // Here we handle various theta chars. Note thata in TI-ASCII, theta is at 0x5B which is "[" in ASCII.
        newName = std::regex_replace(newName, std::regex("(\u03b8|\u0398|\u03F4|\u1DBF)"), "[");

        // TODO: handle names according to _type
        newName = std::regex_replace(newName, std::regex("[^[a-zA-Z0-9]"), "");
        if (newName.length() > sizeof(varname) || newName.empty() || is_numeric(newName.substr(0, 1)))
        {
            throw std::invalid_argument("Invalid name given. 8 chars (A-Z, 0-9, θ) max, starting by a letter or θ.");
        }

        // TODO: again, properly handle names according to _type... (needs to be implemented by each type)
        // Quick hack for now...
        const auto& typeName = _type.getName();
        if (typeName == "Real" || typeName == "Real" || typeName == "Program" || typeName == "ProtectedProgram")
        {
            for (auto & c: newName) c = (char) toupper(c);
        }

        newName = str_pad(newName, sizeof(varname), "\0");
        std::copy(newName.begin(), newName.end(), varname);
    }

    void TIVarFile::var_entry_t::determineFullType()
    {
        _type = TIVarType{typeID};
        if (_type.getName() == "AppVar")
        {
            if (data.size() >= 6)
            {
                if (memcmp(&data[2], STH_PythonAppVar::ID_CODE, 4) == 0
                 || memcmp(&data[2], STH_PythonAppVar::ID_SCRIPT, 4) == 0)
                {
                    _type = TIVarType::createFromName("PythonAppVar");
                }
            }
        }
    }

        /*** Public actions **/

    /**
    * @param    array   data   The array of bytes
    */
    void TIVarFile::setContentFromData(const data_t& data, uint16_t entryIdx)
    {
        if (!data.empty())
        {
            this->entries[entryIdx].data = data;
            this->refreshMetadataFields();
        } else {
            throw std::runtime_error("[Error] No data given");
        }
    }
    void TIVarFile::setContentFromData(const data_t& data)
    {
        setContentFromData(data, 0);
    }

    void TIVarFile::setContentFromString(const std::string& str, const options_t& options, uint16_t entryIdx)
    {
        auto& entry = this->entries[entryIdx];
        entry.data = (entry._type.getHandlers().first)(str, options);
        this->refreshMetadataFields();
    }
    void TIVarFile::setContentFromString(const std::string& str, const options_t& options)
    {
        setContentFromString(str, options, 0);
    }
    void TIVarFile::setContentFromString(const std::string& str)
    {
        setContentFromString(str, {}, 0);
    }

    void TIVarFile::setCalcModel(const TIModel& model)
    {
        this->calcModel = model;
        std::string signature = model.getSig();
        std::copy(signature.begin(), signature.end(), this->header.signature);
    }

    void TIVarFile::setVarName(const std::string& name, uint16_t entryIdx)
    {
        this->entries[entryIdx].setVarName(name);
        this->refreshMetadataFields();
    }
    void TIVarFile::setVarName(const std::string& name)
    {
        this->setVarName(name, 0);
    }

    void TIVarFile::setArchived(bool flag, uint16_t entryIdx)
    {
        if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
        {
            this->entries[entryIdx].setArchived(flag);
            this->refreshMetadataFields();
        } else {
            throw std::runtime_error("[Error] Archived flag not supported on this calculator model");
        }
    }
    void TIVarFile::setArchived(bool flag)
    {
        this->setArchived(flag, 0);
    }


    bool TIVarFile::isCorrupt() const
    {
        return corrupt;
    }

    data_t TIVarFile::getRawContent(uint16_t entryIdx)
    {
        return this->entries[entryIdx].data;
    }
    data_t TIVarFile::getRawContent()
    {
        return this->getRawContent(0);
    }

    std::string TIVarFile::getReadableContent(const options_t& options, uint16_t entryIdx)
    {
        const auto& entry = this->entries[entryIdx];
        return (entry._type.getHandlers().second)(entry.data, options);
    }
    std::string TIVarFile::getReadableContent(const options_t& options)
    {
        return getReadableContent(options, 0);
    }
    std::string TIVarFile::getReadableContent()
    {
        return getReadableContent({}, 0);
    }

    data_t TIVarFile::make_bin_data()
    {
        data_t bin_data;

        // Header
        {
            bin_data.insert(bin_data.end(), this->header.signature, this->header.signature + sizeof(var_header_t::signature));
            bin_data.insert(bin_data.end(), this->header.sig_extra, this->header.sig_extra + sizeof(var_header_t::sig_extra));
            bin_data.insert(bin_data.end(), this->header.comment,   this->header.comment   + sizeof(var_header_t::comment));
            bin_data.push_back((uint8_t) (this->header.entries_len & 0xFF)); bin_data.push_back((uint8_t) ((this->header.entries_len >> 8) & 0xFF));
        }

        // Var entries
        for (const auto& entry : this->entries)
        {
            bin_data.push_back((uint8_t) (entry.meta_length & 0xFF)); bin_data.push_back((uint8_t) ((entry.meta_length >> 8) & 0xFF));
            bin_data.push_back((uint8_t) (entry.data_length & 0xFF)); bin_data.push_back((uint8_t) ((entry.data_length >> 8) & 0xFF));
            bin_data.push_back(entry.typeID);
            bin_data.insert(bin_data.end(), entry.varname, entry.varname + + sizeof(var_entry_t::varname));
            if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
            {
                bin_data.push_back(entry.version);
                bin_data.push_back(entry.archivedFlag);
            }
            bin_data.push_back((uint8_t) (entry.data_length2 & 0xFF)); bin_data.push_back((uint8_t) ((entry.data_length2 >> 8) & 0xFF));
            bin_data.insert(bin_data.end(), entry.data.begin(), entry.data.end());
        }

        return bin_data;
    }

    /**
     * Writes a variable to an actual file on the FS
     * If the variable was already loaded from a file, it will be used and overwritten,
     * except if a specific directory and name are provided.
     *
     * @param   string  directory  Directory to save the file to
     * @param   string  name       Name of the file, without the extension
     * @return  string  the full path
     */
    std::string TIVarFile::saveVarToFile(std::string directory, std::string name)
    {
        std::string fullPath;

        if (this->fromFile && directory.empty())
        {
            fullPath = this->filePath;
        } else {
            if (name.empty())
            {
                name = std::string(this->hasMultipleEntries() ? "GROUP" : (char*)(this->entries[0].varname));
            }
            std::string fileName;
            if (this->hasMultipleEntries())
            {
                fileName = name + ".8xg";
            } else {
                const int extIndex = std::max(0, this->calcModel.getOrderId());
                fileName = name + "." + this->entries[0]._type.getExts()[extIndex];
            }
            if (directory.empty())
            {
                directory = ".";
            }
            fullPath = directory + "/" + fileName;
        }

        return saveVarToFile(fullPath);
    }

    std::string TIVarFile::saveVarToFile(std::string path)
    {
        FILE* handle = fopen(path.c_str(), "wb");
        if (!handle)
        {
            throw std::runtime_error("Can't open the output file");
        }

        this->refreshMetadataFields();

        // Make and write file data
        data_t bin_data = make_bin_data();
        fwrite(&bin_data[0], sizeof(bin_data[0]), bin_data.size(), handle);

        // Write checksum
        const char buf[2] = {(char) (this->computedChecksum & 0xFF), (char) ((this->computedChecksum >> 8) & 0xFF)};
        fwrite(buf, sizeof(char), 2, handle);

        fclose(handle);

        this->corrupt = false;

        return path;
    }

    std::string TIVarFile::saveVarToFile()
    {
        return saveVarToFile("", "");
    }
}

#ifdef __EMSCRIPTEN__
    #include <emscripten/bind.h>
    using namespace emscripten;
    EMSCRIPTEN_BINDINGS(_tivarfile) {

            register_map<std::string, int>("options_t");

            class_<tivars::TIVarFile>("TIVarFile")
                    .function("getHeader"                , &tivars::TIVarFile::getHeader)
                    .function("getVarEntries"            , &tivars::TIVarFile::getVarEntries)
                    .function("getInstanceChecksum"      , &tivars::TIVarFile::getInstanceChecksum)

                    .function("getChecksumValueFromFile" , &tivars::TIVarFile::getChecksumValueFromFile)
                    .function("setContentFromData"       , select_overload<void(const data_t& data)>(&tivars::TIVarFile::setContentFromData))
                    .function("setContentFromString"     , select_overload<void(const std::string&, const options_t&)>(&tivars::TIVarFile::setContentFromString))
                    .function("setContentFromString"     , select_overload<void(const std::string&)>(&tivars::TIVarFile::setContentFromString))
                    .function("setCalcModel"             , &tivars::TIVarFile::setCalcModel)
                    .function("setVarName"               , select_overload<void(const std::string&)>(&tivars::TIVarFile::setVarName))
                    .function("setArchived"              , select_overload<void(bool)>(&tivars::TIVarFile::setArchived))
                    .function("isCorrupt"                , &tivars::TIVarFile::isCorrupt)
                    .function("getRawContent"            , select_overload<data_t(void)>(&tivars::TIVarFile::getRawContent))
                    .function("getReadableContent"       , select_overload<std::string(const options_t&)>(&tivars::TIVarFile::getReadableContent))
                    .function("getReadableContent"       , select_overload<std::string(void)>(&tivars::TIVarFile::getReadableContent))

                    .function("saveVarToFile"            , select_overload<std::string(std::string, std::string)>(&tivars::TIVarFile::saveVarToFile))
                    .function("saveVarToFile"            , select_overload<std::string(std::string)>(&tivars::TIVarFile::saveVarToFile))
                    .function("saveVarToFile"            , select_overload<std::string(void)>(&tivars::TIVarFile::saveVarToFile))

                    .class_function("loadFromFile", &tivars::TIVarFile::loadFromFile)
                    .class_function("createNew", select_overload<tivars::TIVarFile(const tivars::TIVarType&, const std::string&, const tivars::TIModel&)>(&tivars::TIVarFile::createNew))
                    .class_function("createNew", select_overload<tivars::TIVarFile(const tivars::TIVarType&, const std::string&)>(&tivars::TIVarFile::createNew))
                    .class_function("createNew", select_overload<tivars::TIVarFile(const tivars::TIVarType&)>(&tivars::TIVarFile::createNew))
            ;
    }
#endif
