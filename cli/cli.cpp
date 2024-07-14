#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "../src/TypeHandlers/TypeHandlers.h"
#include "../src/TIVarFile.h"
#include "../src/TIModels.h"
#include "../src/TIVarTypes.h"

#include "cxxopts.hpp"

using namespace std;
using namespace tivars;
using namespace tivars::TypeHandlers;

enum FileType
{
    RAW,
    READABLE,
    VARFILE
};

enum FileType getType(const cxxopts::ParseResult& options, const string& filename, const string& option);

int main(int argc, char** argv)
{
    TIModels::initTIModelsArray();
    TIVarTypes::initTIVarTypesArray();

    cxxopts::Options options("tivars_lib_cpp", "A program to interact with TI-z80 calculator files");
    options.add_options()
            ("i,input", "Input file", cxxopts::value<string>())
            ("o,output", "Output file", cxxopts::value<string>())
            ("j,iformat", "Input format (raw|readable|varfile)", cxxopts::value<string>())
            ("k,oformat", "Output format (raw|readable|varfile)", cxxopts::value<string>())
            ("n,name", "Variable name", cxxopts::value<string>())
            ("t,type", "Variable type", cxxopts::value<string>())
            ("m,calc", "Calc. model", cxxopts::value<string>())
            ("c,csv", "Tokens CSV File", cxxopts::value<string>())
            ("l,lang", "Language", cxxopts::value<string>()->default_value("en"))
            ("a,archive", "Archive status", cxxopts::value<bool>())
            ("r,reindent", "Re-indent", cxxopts::value<bool>())
            ("p,prettify", "Prettify", cxxopts::value<bool>())
            ("s,detect_strings", "Detect strings", cxxopts::value<bool>())
            ("h,help", "Print usage");

    try
    {
        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            cout << options.help() << endl;
            return 0;
        }

        string ipath, opath;

        if (!result.count("input"))
        {
            cout << "-i/--input is a required argument." << endl;
            return 1;
        }
        ipath = result["input"].as<string>();

        if (!result.count("output"))
        {
            cout << "-o/--output is a required argument." << endl;
            return 1;
        }
        opath = result["output"].as<string>();

        if (result.count("csv"))
        {
            string csvFilePath = result["csv"].as<string>();
            TH_Tokenized::initTokensFromCSVFilePath(csvFilePath);
        } else {
            TH_Tokenized::initTokens();
        }

        enum FileType iformat = getType(result, ipath, "iformat");
        enum FileType oformat = getType(result, opath, "oformat");

        TIVarType varvarType;

        if (iformat != VARFILE)
        {
            if (!result.count("type"))
            {
                cout << "-t/--type is required when the input is not a varfile." << endl;
                return 1;
            }
            string typeName = result["type"].as<string>();

            try
            {
                varvarType = TIVarType(typeName);
            } catch (std::invalid_argument& e)
            {
                cout << typeName << "is not a valid variable type." << endl;
                cout << "Valid types:";
                for (const auto& type: TIVarTypes::all())
                {
                    cout << " " << type.first;
                }
                cout << endl;
                return 1;
            }
        }

        try
        {
            TIVarFile file = iformat == VARFILE ? TIVarFile::loadFromFile(ipath) : TIVarFile::createNew(varvarType);

            if (result.count("name"))
            {
                string name = result["name"].as<string>();
                file.setVarName(name);
            }

            if (result.count("calc"))
            {
                string modelStr = result["calc"].as<string>();
                try
                {
                    TIModel model{modelStr};
                    file.setCalcModel(model);
                } catch (invalid_argument& e)
                {
                    cout << modelStr << "is not a valid calc model." << endl;
                    cout << "Valid models:";
                    for (const auto& model: TIModels::all())
                    {
                        cout << " " << model.first;
                    }
                    cout << endl;
                    return 1;
                }
            }

            file.setArchived(result["archive"].as<bool>());

            if (iformat == RAW)
            {
                ifstream in(ipath, ios::in | ios::binary);
                if (!in)
                {
                    cout << ipath << ": Failed to open file" << endl;
                    return 1;
                }
                in.seekg(0, ios::end);
                int filesize = in.tellg();
                in.seekg(0, ios::beg);

                data_t data;
                data.resize(filesize + 2);
                data[0] = filesize & 0xFF;
                data[1] = (filesize >> 8) & 0xFF;
                in.read((char*) &data[2], filesize);
                in.close();

                file.setContentFromData(data);
            } else if (iformat == READABLE)
            {
                ifstream in(ipath, ios::in);
                if (!in)
                {
                    cout << ipath << ": Failed to open file" << endl;
                    return 1;
                }

                ostringstream str;
                str << in.rdbuf();
                in.close();

                options_t contentOptions;
                contentOptions["detect_strings"] = result["detect_strings"].as<bool>();

                file.setContentFromString(str.str(), contentOptions);
            }

            switch (oformat)
            {
                case RAW:
                {
                    ofstream out(opath, ios::out | ios::binary);
                    if (!out)
                    {
                        cout << opath << ": Failed to open file" << endl;
                        return 1;
                    }
                    out.write((char*) (&file.getRawContent()[2]), file.getRawContent().size() - 2);
                    break;
                }
                case READABLE:
                {
                    ofstream out(opath, ios::out);
                    if (!out)
                    {
                        cout << opath << ": Failed to open file" << endl;
                        return 1;
                    }

                    options_t contentOptions;
                    contentOptions["reindent"] = result["reindent"].as<bool>();
                    contentOptions["prettify"] = result["prettify"].as<bool>();

                    if (result.count("lang"))
                    {
                        string langStr = result["lang"].as<string>();
                        if (langStr == "en")
                        {
                            contentOptions["lang"] = TH_Tokenized::LANG_EN;
                        } else if (langStr == "fr")
                        {
                            contentOptions["lang"] = TH_Tokenized::LANG_FR;
                        } else
                        {
                            cout << langStr << " is not a valid language code" << endl;
                            cout << "Valid languages: en, fr" << endl;
                            return 1;
                        }
                    }

                    out << file.getReadableContent(contentOptions);
                    break;
                }
                case VARFILE:
                {
                    try
                    {
                        file.saveVarToFile(opath);
                    } catch (runtime_error& e)
                    {
                        cout << opath << ": failed to write file." << endl;
                        return 1;
                    }
                    break;
                }
            }

        } catch (runtime_error& e)
        {
            if ((string) e.what() == "No such file")
            {
                cout << ipath << ": no such file or directory" << endl;
            } else
                throw e;
        }

    } catch (cxxopts::OptionParseException& e)
    {
        cout << options.help() << endl;
        return 0;
    }
}

static unordered_map<string, FileType> const fileTypes = {
        {"raw",      RAW},
        {"readable", READABLE},
        {"varfile",  VARFILE}
};

enum FileType getType(const cxxopts::ParseResult& options, const string& filename, const string& option)
{
    // Type manually specified, use that
    if (options.count(option))
    {
        const string& typeStr = options[option].as<string>();
        const auto& i = fileTypes.find(typeStr);
        if (i != fileTypes.end())
        {
            return i->second;
        } else
        {
            cout << typeStr << " is not a valid file type." << endl;
            cout << "Valid types: raw, readable, varfile" << endl;
            exit(1);
        }
    }

    // File type not specified, guess by file extension
    const auto& pos = filename.find_last_of('.');
    if (pos == string::npos)
    {
        cout << "--" << option << " is required for files without an extension." << endl;
        exit(1);
    }

    string extension = filename.substr(pos + 1);
    transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == "bin")
        return RAW;

    if (extension == "txt")
        return READABLE;

    for (const auto& type: TIVarTypes::all())
    {
        const vector<string>& exts = type.second.getExts();
        if (std::find(exts.begin(), exts.end(), extension) != exts.end())
            return VARFILE;
    }

    cout << "Could not guess file type from file extension. Use --" << option << " to specify file type." << endl;
    exit(1);
}
