/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef DUMMY_HANDLER_H
#define DUMMY_HANDLER_H

#include "../autoloader.h"

namespace tivars
{
    class DummyHandler
    {

    public:

        // We can't make virtual static methods...

        static data_t makeDataFromString(const std::string& str, const options_t options = {})
        {
            throw std::runtime_error("This type is not supported / implemented (yet?)");
        }

        static std::string makeStringFromData(const data_t& data, const options_t options = {})
        {
            throw std::runtime_error("This type is not supported / implemented (yet?)");
        }

    };

    typedef decltype(&DummyHandler::makeDataFromString) dataFromString_handler_t;
    typedef decltype(&DummyHandler::makeStringFromData) stringFromData_handler_t;
    typedef std::pair<dataFromString_handler_t, stringFromData_handler_t> handler_pair_t;

#define make_handler_pair(cls)   make_pair(&cls::makeDataFromString, &cls::makeStringFromData)

}

#endif