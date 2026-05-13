#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "../src/TypeHandlers/TypeHandlers.h"
#include "../src/TIVarFile.h"
#include "../src/TIFlashFile.h"
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
bool isFlashType(const TIVarType& type);
bool isFlashExtension(const string& extension);
string lowercase(string value);

int main(int argc, char** argv)
{
    cxxopts::Options options("tivars_lib_cpp", "A program to interact with TI-z80 calculator files");
    options.add_options()
            ("i,input", "Input file", cxxopts::value<string>())
            ("o,output", "Output file", cxxopts::value<string>())
            ("j,iformat", "Input format (raw|readable|varfile)", cxxopts::value<string>())
            ("k,oformat", "Output format (raw|readable|varfile)", cxxopts::value<string>())
            ("n,name", "Variable name", cxxopts::value<string>())
            ("t,type", "Variable type", cxxopts::value<string>())
            ("m,calc", "Calc. model", cxxopts::value<string>())
            ("x,xml", "Path to tokens XML file (overrides default ti-toolkit-8x-tokens.xml)", cxxopts::value<string>())
            ("l,lang", "Language", cxxopts::value<string>()->default_value("en"))
            ("a,archive", "Archive status", cxxopts::value<bool>())
            ("r,reindent", "Re-indent", cxxopts::value<bool>())
            ("accessible", "Use accessible token names for non-US-keyboard display tokens", cxxopts::value<bool>())
            ("p,prettify", "Prettify (display-oriented, may not roundtrip)", cxxopts::value<bool>())
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
            const bool useFlashFile = (iformat == VARFILE)
                                    ? [&]() {
                                          const auto& pos = ipath.find_last_of('.');
                                          return pos != string::npos && isFlashExtension(ipath.substr(pos + 1));
                                      }()
                                    : isFlashType(varvarType);

            if (useFlashFile)
            {
                TIFlashFile file;

                if (iformat == VARFILE)
                {
                    file = TIFlashFile::loadFromFile(ipath);
                }
                else
                {
                    string flashName;
                    if (result.count("name"))
                    {
                        flashName = result["name"].as<string>();
                    }

                    TIModel model{"84+CE"};
                    if (result.count("calc"))
                    {
                        string modelStr = result["calc"].as<string>();
                        try
                        {
                            model = TIModel{modelStr};
                        } catch (invalid_argument& e)
                        {
                            cout << modelStr << "is not a valid calc model." << endl;
                            cout << "Valid models:";
                            for (const auto& validModel: TIModels::all())
                            {
                                cout << " " << validModel.first;
                            }
                            cout << endl;
                            return 1;
                        }
                    }

                    file = TIFlashFile::createNew(varvarType, flashName, model);
                }

                if (iformat == RAW)
                {
                    file = TIFlashFile::loadFromFile(ipath);
                }
                else if (iformat == READABLE)
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

                    file.setContentFromString(str.str());
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
                        const data_t bytes = file.makeBinData();
                        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
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

                        out << file.getReadableContent();
                        break;
                    }
                    case VARFILE:
                    {
                        try
                        {
                            file.saveToFile(opath);
                        } catch (runtime_error& e)
                        {
                            cout << opath << ": failed to write file." << endl;
                            return 1;
                        }
                        break;
                    }
                }
            }
            else
            {
                if (result.count("xml"))
                {
                    string xmlFilePath = result["xml"].as<string>();
                    TH_Tokenized::initTokensFromXMLFilePath(xmlFilePath);
                } else {
                    TH_Tokenized::initTokens();
                }

                string requestedName;
                if (result.count("name"))
                {
                    requestedName = result["name"].as<string>();
                }

                TIModel requestedModel{"84+CE"};
                if (result.count("calc"))
                {
                    string modelStr = result["calc"].as<string>();
                    try
                    {
                        requestedModel = TIModel{modelStr};
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

                TIVarFile file = iformat == VARFILE
                               ? TIVarFile::loadFromFile(ipath)
                               : TIVarFile::createNew(varvarType, requestedName, requestedModel);

                if (iformat == VARFILE)
                {
                    if (result.count("name"))
                    {
                        file.setVarName(requestedName);
                    }
                    if (result.count("calc"))
                    {
                        file.setCalcModel(requestedModel);
                    }
                }

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

                    data_t data(filesize);
                    in.read((char*) data.data(), filesize);
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

                if (result.count("archive"))
                {
                    file.setArchived(result["archive"].as<bool>());
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
                        const data_t rawContent = file.getRawContent();
                        if (!rawContent.empty())
                        {
                            out.write((char*) rawContent.data(), rawContent.size());
                        }
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
                        contentOptions["accessible"] = result["accessible"].as<bool>();
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
            }

        } catch (runtime_error& e)
        {
            if ((string) e.what() == "No such file")
            {
                cout << ipath << ": no such file or directory" << endl;
                return 1;
            } else
            {
                cout << "Error: " << e.what() << endl;
                return 1;
            }
        } catch (const std::exception& e)
        {
            cout << "Error: " << e.what() << endl;
            return 1;
        }

    } catch (cxxopts::OptionParseException& e)
    {
        cout << options.help() << endl;
        return 0;
    }
}

bool isFlashType(const TIVarType& type)
{
    const string& name = type.getName();
    return name == "OperatingSystem"
        || name == "FlashApp"
        || name == "Certificate"
        || name == "CertificateMemory"
        || name == "UnitCertificate"
        || name == "FlashLicense";
}

string lowercase(string value)
{
    transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value;
}

bool isFlashExtension(const string& extension)
{
    const string lowered = lowercase(extension);
    for (const string& flashExtension : {"82u", "8xu", "8cu", "8eu", "8pu", "8yu", "8xk", "8ck", "8ek", "8xq", "8cq"})
    {
        if (lowered == flashExtension)
        {
            return true;
        }
    }
    return false;
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
