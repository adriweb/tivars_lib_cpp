/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarType.h"
#include "TIVarTypes.h"
#include "utils.h"
#include "TypeHandlers/TH_0x05.h"

using namespace std;

namespace tivars
{
    /*** "Constructors" ***/

    /**
     * @param   int     id     The type ID
     * @return  TIVarType
     * @throws  \Exception
     */
    TIVarType TIVarType::createFromID(uint id)
    {
        if (TIVarTypes::isValidID(id))
        {
            TIVarType varType;
            varType.id = id;
            varType.exts = TIVarTypes::getExtensionsFromTypeID(id);
            varType.name = TIVarTypes::getNameFromID(id);
            return varType;
        } else {
            throw invalid_argument("Invalid type ID");
        }
    }

    /**
     * @param   string  name   The type name
     * @return  TIVarType
     * @throws  \Exception
     */
    TIVarType TIVarType::createFromName(string name)
    {
        if (TIVarTypes::isValidName(name))
        {
            TIVarType varType;
            varType.name = name;
            varType.id   = TIVarTypes::getIDFromName(name);
            varType.exts = TIVarTypes::getExtensionsFromName(name);
            return varType;
        } else {
            throw invalid_argument("Invalid type name");
        }
    }

}
