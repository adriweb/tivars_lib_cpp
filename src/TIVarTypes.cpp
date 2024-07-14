/*
 * Part of tivars_lib_cpp
 * (C) 2015-2022 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarTypes.h"
#include "TIVarFile.h"

namespace tivars
{
    using namespace TypeHandlers;

    namespace
    {
        std::unordered_map<std::string, TIVarType> types;
        const TIVarType unknownVarType{};
    }

    const std::unordered_map<std::string, TIVarType>& TIVarTypes::all()
    {
        return types;
    }

    TIVarType TIVarTypes::fromName(const std::string& name)
    {
        return isValidName(name) ? types[name] : unknownVarType;
    }

    TIVarType TIVarTypes::fromId(uint8_t id)
    {
        return isValidID(id) ? types[std::to_string(id)] : unknownVarType;
    }

// Wrap the makeDataFromStr function by one that adds the type/subtype in the options
// Ideally, the handlers would parse the string and select the correct handler to dispatch...
#define GenericHandlerPair(which, type) make_pair([](const std::string& str, const options_t& options, const TIVarFile* _ctx) -> data_t { \
    options_t options_withType = options;                                                                          \
    options_withType["_type"] = type;                                                                              \
    return (TypeHandlers::TH_Generic##which::makeDataFromString)(str, options_withType, _ctx);                                   \
}, &TypeHandlers::TH_Generic##which::makeStringFromData)

    void TIVarTypes::insertType(const std::string& name, int id, const std::vector<std::string>& exts, const handler_pair_t& handlers)
    {
        const TIVarType varType(id, name, exts, handlers);
        types[name] = varType;
        const std::string id_str = std::to_string(id);
        if (types.count(id_str) == 0)
        {
            types[id_str] = varType;
        }
        for (const std::string& ext : exts)
        {
            if (!ext.empty() && !types.count(ext))
            {
                types[ext] = varType;
            }
        }
    }

    // 82+/83+/84+ are grouped since only the clock is the difference, and it doesn't have an actual varType.
    // For number vartypes, Real and Complex are the generic handlers we use, they'll dispatch to specific ones.
    void TIVarTypes::initTIVarTypesArray() // order: 82     83    82A   84+T  82+/83+  84+C  84+CE  83PCE  82AEP
                                           //                                   84+         84+CE-T
    {
        const std::string _;

        /* Standard types */
        insertType("Real",                 0x00,  {"82n", "83n", "8xn", "8xn", "8xn", "8xn", "8xn", "8xn", "8xn"},  GenericHandlerPair(Real, 0x00)  );
        insertType("RealList",             0x01,  {"82l", "83l", "8xl", "8xl", "8xl", "8xl", "8xl", "8xl", "8xl"},  GenericHandlerPair(List, 0x00)  );
        insertType("Matrix",               0x02,  {"82m", "83m", "8xm", "8xm", "8xm", "8xm", "8xm", "8xm", "8xm"},  make_handler_pair(TH_Matrix)    );
        insertType("Equation",             0x03,  {"82y", "83y", "8xy", "8xy", "8xy", "8xy", "8xy", "8xy", "8xy"},  make_handler_pair(TH_Tokenized) );
        insertType("String",               0x04,  {"82s", "83s", "8xs", "8xs", "8xs", "8xs", "8xs", "8xs", "8xs"},  make_handler_pair(TH_Tokenized) );
        insertType("Program",              0x05,  {"82p", "83p", "8xp", "8xp", "8xp", "8xp", "8xp", "8xp", "8xp"},  make_handler_pair(TH_Tokenized) );
        insertType("ProtectedProgram",     0x06,  {"82p", "83p", "8xp", "8xp", "8xp", "8xp", "8xp", "8xp", "8xp"},  make_handler_pair(TH_Tokenized) );
        insertType("Picture",              0x07,  {"82i", "83i", "8xi", "8xi", "8xi", "8ci", "8ci", "8ci", "8ci"});
        insertType("GraphDataBase",        0x08,  {"82d", "83d", "8xd", "8xd", "8xd", "8xd", "8xd", "8xd", "8xd"},  make_handler_pair(TH_GDB) );
        // insertType("Unknown",              0x09,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        // insertType("UnknownEqu",           0x0A,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("SmartEquation",        0x0B,  {"82y", "83y", "8xy", "8xy", "8xy", "8xy", "8xy", "8xy", "8xy"},  make_handler_pair(TH_Tokenized) ); // aka "New Equation"
        insertType("Complex",              0x0C,  {  _  , "83c", "8xc", "8xc", "8xc", "8xc", "8xc", "8xc", "8xc"},  GenericHandlerPair(Complex, 0x0C) );
        insertType("ComplexList",          0x0D,  {  _  , "83l", "8xl", "8xl", "8xl", "8xl", "8xl", "8xl", "8xl"},  GenericHandlerPair(List,    0x0C) );
        // insertType("Undef",                0x0E,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("WindowSettings",       0x0F,  {"82w", "83w", "8xw", "8xw", "8xw", "8xw", "8xw", "8xw", "8xw"});
        insertType("RecallWindow",         0x10,  {"82z", "83z", "8xz", "8xz", "8xz", "8xz", "8xz", "8xz", "8xz"});
        insertType("TableRange",           0x11,  {"82t", "83t", "8xt", "8xt", "8xt", "8xt", "8xt", "8xt", "8xt"});
        insertType("ScreenImage",          0x12,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("Backup",               0x13,  {"82b", "83b", "8xb",   _  , "8xb", "8cb",   _  ,   _  ,   _  });
        insertType("App",                  0x14,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("AppVar",               0x15,  {  _  ,   _  , "8xv", "8xv", "8xv", "8xv", "8xv", "8xv", "8xv"},  GenericHandlerPair(AppVar, 0x15) );
        insertType("PythonAppVar",         0x15,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xv", "8xv", "8xv"},  make_handler_pair(STH_PythonAppVar) );
        insertType("TemporaryItem",        0x16,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("GroupObject",          0x17,  {"82g", "83g", "8xg", "8xg", "8xg", "8xg", "8cg", "8cg", "8cg"});
        insertType("RealFraction",         0x18,  {  _  ,   _  ,   _  ,   _  , "8xn", "8xn", "8xn", "8xn", "8xn"},  GenericHandlerPair(Real, 0x18) );
        insertType("MixedFraction",        0x19,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("Image",                0x1A,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8ca", "8ca", "8ca"});

        /* Exact values (TI-83 Premium CE [Edition Python] and TI-82 Advanced Edition Python) */
        /* See https://docs.google.com/document/d/1P_OUbnZMZFg8zuOPJHAx34EnwxcQZ8HER9hPeOQ_dtI and especially this lib's implementation */
        insertType("ExactComplexFrac",     0x1B,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xc", "8xc"},  GenericHandlerPair(Complex, 0x1B) );
        insertType("ExactRealRadical",     0x1C,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xn", "8xn"},  GenericHandlerPair(Real,    0x1C) );
        insertType("ExactComplexRadical",  0x1D,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xc", "8xc"},  GenericHandlerPair(Complex, 0x1D) );
        insertType("ExactComplexPi",       0x1E,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xc", "8xc"},  GenericHandlerPair(Complex, 0x1E) );
        insertType("ExactComplexPiFrac",   0x1F,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xc", "8xc"},  GenericHandlerPair(Complex, 0x1F) );
        insertType("ExactRealPi",          0x20,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xn", "8xn"},  GenericHandlerPair(Real,    0x20) );
        insertType("ExactRealPiFrac",      0x21,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  , "8xn", "8xn"},  GenericHandlerPair(Real,    0x21) );

        /* System/Flash-related things */
        // 0x22 - IDList (68k calcs)
        insertType("OperatingSystem",      0x23,  {  _  ,   _  , "82u", "8xu", "8xu", "8cu", "8eu", "8pu", "8yu"});
        insertType("FlashApp",             0x24,  {  _  ,   _  ,   _  ,   _  , "8xk", "8ck", "8ek", "8ek",   _  });
        insertType("Certificate",          0x25,  {  _  ,   _  ,   _  ,   _  , "8xq", "8cq",   _  ,   _  ,   _  });
        insertType("AppIDList",            0x26,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("CertificateMemory",    0x27,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("UnitCertificate",      0x28,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("Clock",                0x29,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
        insertType("FlashLicense",         0x3E,  {  _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  ,   _  });
    }

    bool TIVarTypes::isValidID(uint8_t id)
    {
        return types.count(std::to_string(id));
    }

    bool TIVarTypes::isValidName(const std::string& name)
    {
        return (!name.empty() && types.count(name));
    }
}

//TIVarTypes::initTIVarTypesArray();
