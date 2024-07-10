/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIModels.h"

namespace tivars
{
    namespace
    {
        std::unordered_map<std::string, TIModel> models;
        const TIModel unknownModel{};
    }

    TIModel TIModels::fromName(const std::string& name)
    {
        return isValidName(name) ? models[name] : unknownModel;
    }

    TIModel TIModels::fromSignature(const std::string& sig)
    {
        return isValidSignature(sig) ? models[sig] : unknownModel;
    }

    TIModel TIModels::fromPID(uint8_t pid)
    {
        return isValidPID(pid) ? models[std::to_string(pid)] : unknownModel;
    }

    // orderID is for the extensions association
    void TIModels::insertModel(int orderID, uint32_t flags, const std::string& name, const std::string& sig, uint8_t productId)
    {
        const TIModel model(orderID, name, flags, sig, productId);

        if (!models.count(name))
            models[name] = model;

        const std::string pid_str = std::to_string(productId);
        if (!models.count(pid_str))
            models[pid_str] = model;

        if (!models.count(sig))
            models[sig] = model;
    }

    void TIModels::initTIModelsArray()
    {
        const uint32_t flags82     = 0           | has82things;
        const uint32_t flags83     = flags82     | hasComplex;
        const uint32_t flags83p    = flags83     | hasFlash | hasApps;
        const uint32_t flags84p    = flags83p    | hasClock;
        const uint32_t flags82a    = flags84p    &~hasApps;
        const uint32_t flags84pcse = flags84p    | hasColorLCD;
        const uint32_t flags84pce  = flags84pcse | hasEZ80CPU;
        const uint32_t flags83pce  = flags84pce  | hasExactMath;
        const uint32_t flags83pceep= flags83pce  | hasPython;
        const uint32_t flags84pcepy= flags84pce  | hasPython;
        const uint32_t flags82aep  = flags83pceep&~hasApps;

        // In case of duplicate ProductID for a given orderID, we first insert the default model for that ProductID
        insertModel(0,  flags82,     "82",      "**TI82**", 0);
        insertModel(1,  flags83,     "83",      "**TI83**", 0);
        insertModel(2,  flags82a,    "82A",     "**TI83F*", 0x0B);
        insertModel(3,  flags84p,    "84+T",    "**TI83F*", 0x1B);
        insertModel(4,  flags83p,    "83+",     "**TI83F*", 0x04);
        insertModel(4,  flags83p,    "82+",     "**TI83F*", 0x04);
        insertModel(4,  flags84p,    "84+",     "**TI83F*", 0x0A);
        insertModel(5,  flags84pcse, "84+CSE",  "**TI83F*", 0x0F);
        insertModel(6,  flags84pce,  "84+CE",   "**TI83F*", 0x13);
        insertModel(6,  flags84pce,  "84+CET",  "**TI83F*", 0x13);
        insertModel(6,  flags84pcepy,"84+CETPE","**TI83F*", 0x13);
        insertModel(6,  flags84pcepy,"84+CEPy", "**TI83F*", 0x13);
        insertModel(7,  flags83pce,  "83PCE",   "**TI83F*", 0x13);
        insertModel(7,  flags83pceep,"83PCEEP", "**TI83F*", 0x13);
        insertModel(8,  flags82aep,  "82AEP",   "**TI83F*", 0x15);
    }

    const std::unordered_map<std::string, TIModel>& TIModels::all()
    {
        return models;
    }

    bool TIModels::isValidPID(uint8_t pid)
    {
        return (pid > 0 && models.count(std::to_string(pid)));
    }

    bool TIModels::isValidName(const std::string& name)
    {
        return (!name.empty() && models.count(name));
    }

    bool TIModels::isValidSignature(const std::string& sig)
    {
        return (!sig.empty() && models.count(sig));
    }

};

// TIModels::initTIModelsArray();