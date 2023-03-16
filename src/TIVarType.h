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
        /*** "Constructors" ***/
        static TIVarType createFromID(uint8_t id);
        static TIVarType createFromName(const std::string& name);

        TIVarType() = default;

        explicit TIVarType(uint8_t id) { *this = createFromID(id); }
        TIVarType(const std::string& name) { *this = createFromName(name); }
        TIVarType(const char* name) { *this = createFromName(name); }

        TIVarType(int id, const std::string& name, const std::vector<std::string>& exts, const handler_pair_t& handlers) : id(id), name(name), exts(exts), handlers(handlers)
        {}

        ~TIVarType() = default;

        /* Getters */
        int getId() const { return this->id; }
        std::string getName() const { return this->name; }
        const std::vector<std::string>& getExts() const { return this->exts; }
        const handler_pair_t& getHandlers() const { return this->handlers; };

    private:
        int id = -1;
        std::string name = "Unknown";
        std::vector<std::string> exts;
        handler_pair_t handlers;

    };

}

#endif
