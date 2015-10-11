/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarFile.h"
#include "utils.h"
#include "TIModels.h"
#include "TypeHandlers/TH_0x05.h"
#include "TypeHandlers/TH_0x00.h"
#include "TypeHandlers/TypeHandlerFuncDispatcher.h"
#include <regex>
#include <numeric>

using namespace std;

namespace tivars
{

    /*** Constructors ***/

    /**
     * Internal constructor, called from loadFromFile and createNew.
     * @param   string  filePath
     * @throws  \Exception
     */
    TIVarFile::TIVarFile(const string filePath) : BinaryFile(filePath)
    {
        if (filePath != "")
        {
            this->isFromFile = true;
            if (this->fileSize < 76) // bare minimum for header + a var entry
            {
                throw runtime_error("This file is not a valid TI-[e]z80 variable file");
            }
            this->makeHeaderFromFile();
            this->makeVarEntryFromFile();
            this->computedChecksum = this->computeChecksumFromFileData();
            this->inFileChecksum = this->getChecksumValueFromFile();
            this->type = TIVarType::createFromID(this->varEntry.typeID);
        } else {
            this->isFromFile = false;
        }
    }

    TIVarFile TIVarFile::loadFromFile(const string filePath)
    {
        if (filePath != "")
        {
            TIVarFile varFile(filePath);
            return varFile;
        } else {
            throw runtime_error("No file path given");
        }
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const string name, const TIModel& version)
    {
        string newName(name);
        if (newName == "")
        {
            newName = "FILE" + ((type.getExts().size() > 0) ? type.getExts()[0] : "");
        }
        newName = regex_replace(newName, regex("[^a-zA-Z0-9]"), "");
        if (newName.length() > 8 || newName == "" || is_numeric(newName.substr(0, 1)))
        {
            throw invalid_argument("Invalid name given. 8 chars (A-Z, 0-9) max, starting by a letter");
        }

        for (auto & c: newName) c = (char) toupper(c);

        TIVarFile varFile;
        varFile.type = type;
        varFile.calcModel = version;

        if (!varFile.calcModel.supportsType(varFile.type))
        {
            throw runtime_error("This calculator model (" + varFile.calcModel.getName() + ") does not support the type " + varFile.type.getName());
        }

        string signature = varFile.calcModel.getSig();
        std::copy(signature.begin(), signature.end(), varFile.header.signature);
        uchar sig_extra[3] = {0x1A, 0x0A, 0x00};
        std::copy(sig_extra, sig_extra + 3, varFile.header.sig_extra);
        string comment = str_pad("Created by tivars_lib_cpp", 42, "\0");
        std::copy(comment.begin(), comment.end(), varFile.header.comment);
        varFile.header.entries_len = 0; // will have to be overwritten later

        uint calcFlags = varFile.calcModel.getFlags();

        uchar constBytes[2] = {0x0D, 0x00};
        std::copy(constBytes, constBytes + 2, varFile.varEntry.constBytes);
        varFile.varEntry.data_length  = 0; // will have to be overwritten later
        varFile.varEntry.typeID       = (uchar) type.getId();
        string varname = str_pad(newName, 8, "\0");
        std::copy(varname.begin(), varname.end(), varFile.varEntry.varname);
        varFile.varEntry.version      = (calcFlags >= TIFeatureFlags::hasFlash) ? (uchar)0 : (uchar)-1;
        varFile.varEntry.archivedFlag = (calcFlags >= TIFeatureFlags::hasFlash) ? (uchar)0 : (uchar)-1; // TODO: check when that needs to be 1.
        varFile.varEntry.data_length2 = 0; // will have to be overwritten later

        return varFile;
    }

    TIVarFile TIVarFile::createNew(const TIVarType& type, const string name)
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

        string signature = this->get_string_bytes(8);
        std::copy(signature.begin(), signature.end(), this->header.signature);
        auto sig_extra = this->get_raw_bytes(3);
        std::copy(sig_extra.begin(), sig_extra.end(), this->header.sig_extra);
        string comment = this->get_string_bytes(42);
        std::copy(comment.begin(), comment.end(), this->header.comment);
        this->header.entries_len = (uint16_t)(this->get_raw_bytes(1)[0] & 0xFF);
        this->header.entries_len += (uint16_t)(this->get_raw_bytes(1)[0] << 8);
        this->calcModel = TIModel::createFromSignature(signature);
    }

    void TIVarFile::makeVarEntryFromFile()
    {
        uint calcFlags = this->calcModel.getFlags();
        long dataSectionOffset = (8+3+42+2); // after header
        fseek(this->file, dataSectionOffset, SEEK_SET);

        auto constBytes             = this->get_raw_bytes(2);
        std::copy(constBytes.begin(), constBytes.end(), this->varEntry.constBytes);
        this->varEntry.data_length  = this->get_raw_bytes(1)[0] + (this->get_raw_bytes(1)[0] << 8);
        this->varEntry.typeID       = this->get_raw_bytes(1)[0];
        string varname              = this->get_string_bytes(8);
        std::copy(varname.begin(), varname.end(), this->varEntry.varname);
        this->varEntry.version      = (calcFlags >= TIFeatureFlags::hasFlash) ? this->get_raw_bytes(1)[0] : (uchar)-1;
        this->varEntry.archivedFlag = (calcFlags >= TIFeatureFlags::hasFlash) ? this->get_raw_bytes(1)[0] : (uchar)-1;
        this->varEntry.data_length2 = this->get_raw_bytes(1)[0] + (this->get_raw_bytes(1)[0] << 8);
        this->varEntry.data         = this->get_raw_bytes(this->varEntry.data_length);
    }

    /*** Utils. ***/

    bool TIVarFile::isValid()
    {
        return (this->isFromFile) ? (this->computedChecksum == this->inFileChecksum)
                                  : (this->computedChecksum != 0);
    }


    /*** Private actions ***/

    uint16_t TIVarFile::computeChecksumFromFileData()
    {
        if (this->isFromFile)
        {
            long dataSectionOffset = (8 + 3 + 42 + 2); // after header
            fseek(this->file, dataSectionOffset, SEEK_SET);
            uint16_t sum = 0;
            for (long i = dataSectionOffset; i < this->fileSize - 2; i++)
            {
                sum += this->get_raw_bytes(1)[0];
            }
            return (uint16_t) (sum & 0xFFFF);
        } else {
            throw runtime_error("[Error] No file loaded");
        }
    }

    uint16_t TIVarFile::computeChecksumFromInstanceData()
    {
        uint16_t sum = 0;
        sum += std::accumulate(this->varEntry.constBytes, this->varEntry.constBytes + 2, 0);
        sum += 2 * ((this->varEntry.data_length & 0xFF) + ((this->varEntry.data_length >> 8) & 0xFF));
        sum += this->varEntry.typeID + (int)this->varEntry.version + (int)this->varEntry.archivedFlag;
        sum += std::accumulate(this->varEntry.varname, this->varEntry.varname + 8, 0);
        for (uint16_t i=0; i<this->varEntry.data_length; i++)
        {
            sum += this->varEntry.data[i];
        }
        return (uint16_t) (sum & 0xFFFF);
    }

    uint16_t TIVarFile::getChecksumValueFromFile()
    {
        if (this->isFromFile)
        {
            fseek(this->file, this->fileSize - 2, SEEK_SET);
            return this->get_raw_bytes(1)[0] + (this->get_raw_bytes(1)[0] << 8);
        } else {
            throw runtime_error("[Error] No file loaded");
        }
    }

    /**
     *  Updates the length fields in both the header and the var entry, as well as the checksum
     */
    void TIVarFile::refreshMetadataFields()
    {
        this->varEntry.data_length = this->varEntry.data_length2 = (uint16_t) this->varEntry.data.size();
        this->header.entries_len = (uint16_t) (this->varEntry.data_length + 17); // 17 == sum of the individual sizes.
        this->computedChecksum = this->computeChecksumFromInstanceData();
    }


    /*** Public actions **/

    /**
    * @param    array   data   The array of bytes
    */
    void TIVarFile::setContentFromData(const data_t data)
    {
        if (data.size() > 0)
        {
            this->varEntry.data = data;
            this->refreshMetadataFields();
        } else {
            throw runtime_error("[Error] No data given");
        }
    }

    void TIVarFile::setContentFromString(const string str, const options_t options)
    {
        auto func = TypeHandlerFuncDispatcher::getDataFromStringFunc(this->type.getId());
        this->varEntry.data = func(str, options);
        this->refreshMetadataFields();
    }

    data_t TIVarFile::getRawContent()
    {
        return this->varEntry.data;
    }

    string TIVarFile::getReadableContent(const options_t options)
    {
        auto func = TypeHandlerFuncDispatcher::getStringFromDataFunc(this->type.getId());
        return func(this->varEntry.data, options);
    }

    void TIVarFile::fixChecksumInFile()
    {
        if (this->isFromFile)
        {
            if (!this->isValid())
            {
                fseek(this->file, this->fileSize - 2, SEEK_SET);
                char buf[2] = {(char) (this->computedChecksum & 0xFF), (char) ((this->computedChecksum >> 8) & 0xFF)};
                fwrite(buf, sizeof(char), sizeof(buf), this->file);
                this->inFileChecksum = this->getChecksumValueFromFile();
            }
        } else {
            throw runtime_error("[Error] No file loaded");
        }
    }

    /**
     * Writes a variable to an actual file on the FS
     * If the variable was already loaded from a file, it will be used and overwritten,
     * except if a specific directory and name are provided.
     *
     * @param   string  directory  Directory to save the file to
     * @param   string  name       Name of the file, without the extension
     */
    // TODO
    void TIVarFile::saveVarToFile(const string directory, const string name)
    {
        /*
        if (this->isFromFile && directory == "")
        {
            this->close();
            handle = fopen(this->filePath, "wb");
        } else {
            if (name == "")
            {
                name = this->varEntry.varname;
            }
            // TODO: make user be able to precise for which model the extension will be fitted
            fileName = str_replace("\0", "", name) + "." + this->getType()->getExts()[0];
            if (directory == "")
            {
                directory = "./";
            }
            directory = rtrim(directory, "/");
            fullPath = realpath(directory) + "/" + fileName;
            handle = fopen(fullPath, "wb");
        }

        this->refreshMetadataFields();

        bin_data = "";
        foreach ([this->header, this->varEntry] as whichData)
        {
            foreach (whichData as key => data)
            {
                // fields not used for this calc version, for instance.
                // TODO : check with (uchar)-1
                if (data == NULL)
                {
                    continue;
                }
                switch (gettype(data))
                {
                    case "integer":
                        // The length fields are the only ones on 2 bytes.
                        if (key == "entries_len" || key == "data_length" || key == "data_length2")
                        {
                            bin_data .= chr(data & 0xFF) + chr((data >> 8) & 0xFF);
                        } else {
                            bin_data .= chr(data & 0xFF);
                        }
                        break;
                    case "string":
                        bin_data .= data;
                        break;
                    case "array":
                        foreach (data as subData)
                        {
                            bin_data .= chr(subData & 0xFF);
                        }
                        break;
                }
            }
        }

        fwrite(handle, bin_data);
        fwrite(handle, chr(this->computedChecksum & 0xFF) + chr((this->computedChecksum >> 8) & 0xFF));

        fclose(handle);
         */
    }

}