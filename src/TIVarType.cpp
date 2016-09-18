/*
 * Part of tivars_lib_cpp
 * (C) 2015-2016 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarType.h"
#include "TIVarTypes.h"
#include "utils.h"
#include "TypeHandlers/TypeHandlers.h"

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
            return types.at(to_string(id));
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
            return types.at(name);
        } else {
            throw invalid_argument("Invalid type name");
        }
    }

}
