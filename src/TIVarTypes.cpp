/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarTypes.h"
#include "TIVarType.h"
#include "utils.h"

using namespace std;

namespace tivars
{

    unordered_map<string, TIVarType> types;

    /**
     *  Make and insert the associative arrays for the type.
     *
     * @param string    name   The name of the type
     * @param int       id     The ID of the type
     * @param array     exts   The extensions the type can have, ordered by feature flags.
     */
    void TIVarTypes::insertType(string name, int id, vector<string> exts)
    {
        TIVarType varType(id, name, exts);

        string id_str = to_string(id);
        types[name]   = varType;
        types[id_str] = varType;
        for (const string ext : exts)
        {
            if (ext != "" && !is_in_umap_string_TIVarType(types, ext))
            {
                types[ext] = varType;
            }
        }
    }

    // 82+/83+/84+ are grouped since only the clock is the difference, and it doesn't have an actual varType.
    void TIVarTypes::initTIVarTypesArray() // order: 82     83   82A 82+/83+/84+ 84+CSE 84+CE 83PCE
    {
        insertType("Unknown",                -1,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" ,   "" });

        /* Standard types */
        insertType("Real",                 0x00,  {"82n", "83n", "8xn", "8xn", "8xn", "8xn", "8xn"});
        insertType("RealList",             0x01,  {"82l", "83l", "8xl", "8xl", "8xl", "8xl", "8xl"});
        insertType("Matrix",               0x02,  {"82m", "83m", "8xm", "8xm", "8xm", "8xm", "8xm"});
        insertType("Equation",             0x03,  {"82y", "83y", "8xy", "8xy", "8xy", "8xy", "8xy"});
        insertType("String",               0x04,  {"82s", "83s", "8xs", "8xs", "8xs", "8xs", "8xs"});
        insertType("Program",              0x05,  {"82p", "83p", "8xp", "8xp", "8xp", "8xp", "8xp"});
        insertType("ProtectedProgram",     0x06,  {"82p", "83p", "8xp", "8xp", "8xp", "8xp", "8xp"});
        insertType("Picture",              0x07,  {  "" ,   "" , "8xi", "8xi", "8ci", "8ci", "8ci"});
        insertType("GraphDataBase",        0x08,  {"82d", "83d", "8xd", "8xd", "8xd", "8xd", "8xd"});
//      insertType("WindowSettings",       0x0B,  {"82w", "83w", "8xw", "8xw", "8xw", "8xw", "8xw"});
        insertType("Complex",              0x0C,  {  "" , "83c", "8xc", "8xc", "8xc", "8xc", "8xc"});
        insertType("ComplexList",          0x0D,  {  "" , "83l", "8xl", "8xl", "8xl", "8xl", "8xl"});
        insertType("WindowSettings",       0x0F,  {"82w", "83w", "8xw", "8xw", "8xw", "8xw", "8xw"});
        insertType("RecallWindow",         0x10,  {"82z", "83z", "8xz", "8xz", "8xz", "8xz", "8xz"});
        insertType("TableRange",           0x11,  {"82t", "83t", "8xt", "8xt", "8xt", "8xt", "8xt"});
        insertType("Backup",               0x13,  {"82b", "83b",   "" , "8xb", "8cb",   "" ,   "" });
        insertType("AppVar",               0x15,  {  "" ,   "" ,   "" , "8xv", "8xv", "8xv", "8xv"});
        insertType("TemporaryItem",        0x16,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" ,   "" });
        insertType("GroupObject",          0x17,  {"82g", "83g", "8xg", "8xg", "8xg", "8cg", "8cg"});
        insertType("RealFration",          0x18,  {  "" ,   "" ,   "" , "8xn", "8xn", "8xn", "8xn"});
        insertType("Image",                0x1A,  {  "" ,   "" ,   "" ,   "" ,   "" , "8ca", "8ca"});

        /* Exact values (TI-83 Premium CE) */
        /* See https://docs.google.com/document/d/1P_OUbnZMZFg8zuOPJHAx34EnwxcQZ8HER9hPeOQ_dtI */
        insertType("ExactComplexFrac",     0x1B,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" , "8xc"});
        insertType("ExactRealRadical",     0x1C,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" , "8xn"});
        insertType("ExactComplexRadical",  0x1D,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" , "8xc"});
        insertType("ExactComplexPi",       0x1E,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" , "8xc"});
        insertType("ExactComplexPiFrac",   0x1F,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" , "8xc"});
        insertType("ExactRealPi",          0x20,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" , "8xn"});
        insertType("ExactRealPiFrac",      0x21,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" , "8xn"});

        /* System/Flash-related things */
        insertType("OperatingSystem",      0x23,  {"82u", "83u", "82u", "8xu", "8cu", "8eu", "8pu"});
        insertType("FlashApp",             0x24,  {  "" ,   "" ,   "" , "8xk", "8ck", "8ek", "8ek"});
        insertType("Certificate",          0x25,  {  "" ,   "" ,   "" , "8xq", "8cq",   "" ,   "" });
        insertType("CertificateMemory",    0x27,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" ,   "" });
        insertType("Clock",                0x29,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" ,   "" });
        insertType("FlashLicense",         0x3E,  {  "" ,   "" ,   "" ,   "" ,   "" ,   "" ,   "" });

        // WindowSettings clone thing
        types["0x0B"] = types["0x0F"];
    }

    /**
     * @param   int     id     The type ID
     * @return  string          The type name for that ID
     */
    string TIVarTypes::getNameFromID(int id)
    {
        string id_str = to_string(id);
        if (id != -1 && is_in_umap_string_TIVarType(types, id_str))
        {
            return types[id_str].getName();
        } else {
            return "Unknown";
        }
    }

    /**
     * @param   string  name   The type name
     * @return  int             The type ID for that name
     */
    int TIVarTypes::getIDFromName(string name)
    {
        if (name != "" && is_in_umap_string_TIVarType(types, name))
        {
            return types[name].getId();
        } else {
            return -1;
        }
    }

    /**
     * @param   int     id     The type ID
     * @return  string[]        The array of extensions for that ID
     */
    vector<string> TIVarTypes::getExtensionsFromTypeID(int id)
    {
        string id_str = to_string(id);
        if (id != -1 && is_in_umap_string_TIVarType(types, id_str))
        {
            return types[id_str].getExts();
        } else {
            return {};
        }
    }

    /**
     * @param   string  name
     * @return  string[]        The array of extensions for that ID
     */
    vector<string> TIVarTypes::getExtensionsFromName(string name)
    {
        if (name != "" && is_in_umap_string_TIVarType(types, name))
        {
            return types[name].getExts();
        } else {
            return {};
        }
    }

    bool TIVarTypes::isValidID(int id)
    {
        string id_str = to_string(id);
        return (id != -1 && is_in_umap_string_TIVarType(types, id_str));
    }

    bool TIVarTypes::isValidName(string name)
    {
        return (name != "" && is_in_umap_string_TIVarType(types, name));
    }
}

//TIVarTypes::initTIVarTypesArray();
