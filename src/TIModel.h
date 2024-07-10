/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIMODEL_H
#define TIMODEL_H

#include "CommonTypes.h"
#include "TIVarType.h"

namespace tivars
{

    class TIModel
    {

    public:
        TIModel() = default;
        TIModel(const std::string& name);
        TIModel(const char* name) { *this = TIModel{std::string{name}}; };

        TIModel(int orderId, const std::string& name, uint32_t flags, const std::string& sig, uint8_t productId)
        : orderID(orderId), name(name), flags(flags), sig(sig), productId(productId)
        {}

        ~TIModel() = default;

        /* Getters */
        int getOrderId() const { return this->orderID; }
        int getProductId() const { return this->productId; }
        std::string getName() const { return this->name; }
        uint32_t getFlags() const { return this->flags; }
        std::string getSig() const { return this->sig; }

        bool supportsType(const TIVarType& type) const;

    private:
        int orderID      = -1;
        std::string name = "Unknown";
        uint32_t flags   = 0;
        std::string sig  = "";
        uint8_t productId = 0;

    };

}

#endif
