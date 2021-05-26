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
        this->makeVarEntryFromFile();
        this->computedChecksum = this->computeChecksumFromFileData();
        this->fileChecksum = this->getChecksumValueFromFile();
        if (this->computedChecksum != this->fileChecksum)
        {
            // puts("[Warning] File is corrupt (read and calculated checksums differ)");
            this->corrupt = true;
        }
        this->type = TIVarType::createFromID(this->varEntry.typeID);
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

    TIVarFile::TIVarFile(const TIVarType& type, const std::string& name, const TIModel& model) : type(type), calcModel(model)
    {
        if (!this->calcModel.supportsType(this->type))
        {
            throw std::runtime_error("This calculator model (" + this->calcModel.getName() + ") does not support the type " + this->type.getName());
        }

        const std::string signature = this->calcModel.getSig();
        const std::string varname = str_pad(this->fixVarName(name), sizeof(this->varEntry.varname), "\0");
        const std::string comment = str_pad("Created by tivars_lib_cpp", sizeof(this->header.comment), "\0");

        std::copy(signature.begin(), signature.end(), this->header.signature);
        std::copy(comment.begin(), comment.end(), this->header.comment);
        this->header.entries_len = 0; // will have to be overwritten later

        this->varEntry.meta_length  = (this->calcModel.getFlags() & TIFeatureFlags::hasFlash) ? varEntryNewLength : varEntryOldLength;
        this->varEntry.data_length  = 0; // will have to be overwritten later
        this->varEntry.typeID       = (uint8_t) type.getId();
        std::copy(varname.begin(), varname.end(), this->varEntry.varname);
        this->varEntry.data_length2 = 0; // will have to be overwritten later
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const std::string& name, const TIModel& model)
    {
        return TIVarFile(type, name, model);
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const std::string& name)
    {
        return createNew(type, name, TIModel::createFromName("84+"));
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type)
    {
        return createNew(type, "");
    }

    /*** Makers ***/

    void TIVarFile::makeHeaderFromFile()
    {
        rewind(this->file);

        const auto signature = this->get_string_bytes(sizeof(this->header.signature));
        const auto sig_extra = this->get_raw_bytes(sizeof(this->header.sig_extra));
        const auto comment = this->get_string_bytes(sizeof(this->header.comment));
        std::copy(signature.begin(), signature.end(), this->header.signature);
        std::copy(sig_extra.begin(), sig_extra.end(), this->header.sig_extra);
        std::copy(comment.begin(), comment.end(), this->header.comment);
        this->header.entries_len  = this->get_two_bytes_swapped();
        this->calcModel = TIModel::createFromSignature(signature); // TODO: check sig_extra bytes instead/too since it has the PID which may be more precise
    }

    void TIVarFile::makeVarEntryFromFile()
    {
        fseek(this->file, TIVarFile::dataSectionOffset, SEEK_SET);

        this->varEntry.meta_length  = this->get_two_bytes_swapped();
        this->varEntry.data_length  = this->get_two_bytes_swapped();
        this->varEntry.typeID       = this->get_raw_byte();
        const std::string varname   = this->get_string_bytes(sizeof(this->varEntry.varname));
        std::copy(varname.begin(), varname.end(), this->varEntry.varname);
        if (this->varEntry.meta_length == varEntryNewLength)
        {
            if ((this->calcModel.getFlags() & TIFeatureFlags::hasFlash) == 0)
            {
                fprintf(stderr, "Something is wrong with your file... The var entry meta length indicates is has flash-related fields, but the signature doesn't match...\n");
            }
            this->varEntry.version      = this->get_raw_byte();
            this->varEntry.archivedFlag = this->get_raw_byte();
        }
        else if (this->varEntry.meta_length != varEntryOldLength)
        {
            throw std::invalid_argument("Invalid file. The var entry meta length has an unexpected value. Don't know what to do with that file.");
        }
        this->varEntry.data_length2  = this->get_two_bytes_swapped();
        this->varEntry.data          = this->get_raw_bytes(this->varEntry.data_length);
    }


    /*** Private actions ***/

    uint16_t TIVarFile::computeChecksumFromFileData()
    {
        if (this->fromFile)
        {
            fseek(this->file, TIVarFile::dataSectionOffset, SEEK_SET);

            uint16_t sum = 0;
            for (size_t i = dataSectionOffset; i < this->fileSize - 2; i++)
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
        sum += this->varEntry.meta_length;
        sum += this->varEntry.typeID;
        sum += 2 * ((this->varEntry.data_length & 0xFF) + ((this->varEntry.data_length >> 8) & 0xFF)); // 2* because of the two same length fields
        sum += std::accumulate(this->varEntry.varname, this->varEntry.varname + sizeof(this->varEntry.varname), 0);
        sum += std::accumulate(this->varEntry.data.begin(), this->varEntry.data.end(), 0);
        if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
        {
            sum += this->varEntry.version;
            sum += this->varEntry.archivedFlag;
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
     *  Updates the length fields in both the header and the var entry, as well as the checksum
     */
    void TIVarFile::refreshMetadataFields()
    {
        // todo : recompute correctly for multiple var entries
        this->varEntry.data_length2 = this->varEntry.data_length = (uint16_t) this->varEntry.data.size();

        this->header.entries_len = (uint16_t)sizeof(var_entry_t::data_length) + (uint16_t)sizeof(var_entry_t::data_length2) + this->varEntry.meta_length
                                 + this->varEntry.data_length;

        this->computedChecksum = this->computeChecksumFromInstanceData();
    }

    std::string TIVarFile::fixVarName(const std::string& name)
    {
        std::string newName(name);
        if (newName.empty())
        {
            newName = "FILE" + (type.getExts().empty() ? "" : type.getExts()[0]);
        }
        newName = std::regex_replace(newName, std::regex("[^a-zA-Z0-9]"), "");
        if (newName.length() > sizeof(var_entry_t::varname) || newName.empty() || is_numeric(newName.substr(0, 1)))
        {
            throw std::invalid_argument("Invalid name given. 8 chars (A-Z, 0-9) max, starting by a letter");
        }

        for (auto & c: newName) c = (char) toupper(c);

        newName = str_pad(newName, sizeof(var_entry_t::varname), "\0");

        return newName;
    }

    /*** Public actions **/

    /**
    * @param    array   data   The array of bytes
    */
    void TIVarFile::setContentFromData(const data_t& data)
    {
        if (!data.empty())
        {
            this->varEntry.data = data;
            this->refreshMetadataFields();
        } else {
            throw std::runtime_error("[Error] No data given");
        }
    }

    void TIVarFile::setContentFromString(const std::string& str, const options_t& options)
    {
        this->varEntry.data = (this->type.getHandlers().first)(str, options);
        this->refreshMetadataFields();
    }
    void TIVarFile::setContentFromString(const std::string& str)
    {
        setContentFromString(str, {});
    }

    void TIVarFile::setCalcModel(const TIModel& model)
    {
        this->calcModel = model;
        std::string signature = model.getSig();
        std::copy(signature.begin(), signature.end(), this->header.signature);
    }

    void TIVarFile::setVarName(const std::string& name)
    {
        std::string varname = TIVarFile::fixVarName(name);
        std::copy(varname.begin(), varname.end(), this->varEntry.varname);
        this->refreshMetadataFields();
    }

    void TIVarFile::setArchived(bool flag)
    {
        if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
        {
            this->varEntry.archivedFlag = (uint8_t)(flag ? 1 : 0);
            this->refreshMetadataFields();
        } else {
            throw std::runtime_error("[Error] Archived flag not supported on this calculator model");
        }
    }


    bool TIVarFile::isCorrupt() const
    {
        return corrupt;
    }

    data_t TIVarFile::getRawContent()
    {
        return this->varEntry.data;
    }

    std::string TIVarFile::getReadableContent(const options_t& options)
    {
        return (this->type.getHandlers().second)(this->varEntry.data, options);
    }
    std::string TIVarFile::getReadableContent()
    {
        return getReadableContent({});
    }

    data_t TIVarFile::make_bin_data()
    {
        data_t bin_data;

        // Header
        {
            bin_data.insert(bin_data.end(), this->header.signature, this->header.signature + sizeof(this->header.signature));
            bin_data.insert(bin_data.end(), this->header.sig_extra, this->header.sig_extra + sizeof(this->header.sig_extra));
            bin_data.insert(bin_data.end(), this->header.comment,   this->header.comment   + sizeof(this->header.comment));
            bin_data.push_back((uint8_t) (this->header.entries_len & 0xFF)); bin_data.push_back((uint8_t) ((this->header.entries_len >> 8) & 0xFF));
        }

        // Var entry
        {
            bin_data.push_back((uint8_t) (this->varEntry.meta_length & 0xFF)); bin_data.push_back((uint8_t) ((this->varEntry.meta_length >> 8) & 0xFF));
            bin_data.push_back((uint8_t) (this->varEntry.data_length & 0xFF)); bin_data.push_back((uint8_t) ((this->varEntry.data_length >> 8) & 0xFF));
            bin_data.push_back(this->varEntry.typeID);
            bin_data.insert(bin_data.end(), this->varEntry.varname, this->varEntry.varname + + sizeof(this->varEntry.varname));
            if (this->calcModel.getFlags() & TIFeatureFlags::hasFlash)
            {
                bin_data.push_back(this->varEntry.version);
                bin_data.push_back(this->varEntry.archivedFlag);
            }
            bin_data.push_back((uint8_t) (this->varEntry.data_length2 & 0xFF)); bin_data.push_back((uint8_t) ((this->varEntry.data_length2 >> 8) & 0xFF));
            bin_data.insert(bin_data.end(), this->varEntry.data.begin(), this->varEntry.data.end());
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
                std::string tmp;
                for (unsigned char c : this->varEntry.varname)
                {
                    if (c)
                    {
                        tmp += c;
                    } else {
                        break;
                    }
                }
                name = tmp;
            }
            int extIndex = this->calcModel.getOrderId();
            if (extIndex < 0)
            {
                extIndex = 0;
            }
            std::string fileName = name + "." + this->getType().getExts()[extIndex];
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
        fwrite(&bin_data[0], bin_data.size(), sizeof(uint8_t), handle);

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
                    .function("getVarEntry"              , &tivars::TIVarFile::getVarEntry)
                    .function("getType"                  , &tivars::TIVarFile::getType)
                    .function("getInstanceChecksum"      , &tivars::TIVarFile::getInstanceChecksum)

                    .function("getChecksumValueFromFile" , &tivars::TIVarFile::getChecksumValueFromFile)
                    .function("setContentFromData"       , &tivars::TIVarFile::setContentFromData)
                    .function("setContentFromString"     , select_overload<void(const std::string&, const options_t&)>(&tivars::TIVarFile::setContentFromString))
                    .function("setContentFromString"     , select_overload<void(const std::string&)>(&tivars::TIVarFile::setContentFromString))
                    .function("setCalcModel"             , &tivars::TIVarFile::setCalcModel)
                    .function("setVarName"               , &tivars::TIVarFile::setVarName)
                    .function("setArchived"              , &tivars::TIVarFile::setArchived)
                    .function("isCorrupt"                , &tivars::TIVarFile::isCorrupt)
                    .function("getRawContent"            , &tivars::TIVarFile::getRawContent)
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
