/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARTYPE_H
#define TIVARTYPE_H

#include "CommonTypes.h"
#include "TypeHandlers/TypeHandlers.h"

namespace tivars
{

    class TIVarType
    {

    public:
        TIVarType() = default;

        explicit TIVarType(uint8_t id);
        TIVarType(const std::string& name);
        TIVarType(const char* name) { *this = TIVarType{std::string{name}}; }

        static TIVarType createFromID(uint8_t id) { return TIVarType{id}; }
        static TIVarType createFromName(const std::string& name) { return TIVarType{name}; }

        TIVarType(int id, const std::string& name, const std::vector<std::string>& exts, const TypeHandlers::TypeHandlersTuple& handlers)
        : id(id), name(name), exts(exts), handlers(handlers)
        {}

        ~TIVarType() = default;

        /* Getters */
        int getId() const { return this->id; }
        std::string getName() const { return this->name; }
        const std::vector<std::string>& getExts() const { return this->exts; }
        TypeHandlers::TypeHandlersTuple getHandlers() const { return this->handlers; };

    private:
        int id = -1;
        std::string name = "Unknown";
        std::vector<std::string> exts;
        TypeHandlers::TypeHandlersTuple handlers;

    };

}

#endif
