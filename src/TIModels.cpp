/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIModels.h"
#include <unordered_map>

namespace tivars
{

    std::unordered_map<std::string, TIModel>  models;

    /**
     *  Make and insert the associative arrays for the model.
     *
     * @param int|null  orderID The orderID (for the extensions association)
     * @param uint32_t  flags   The flags determining available features
     * @param string    name    The name of the calc using this model
     * @param string    sig     The signature (magic bytes) used for this model
     */
    void TIModels::insertModel(int orderID, uint32_t flags, const std::string& name, const std::string& sig)
    {
        const TIModel model(orderID, name, flags, sig);

        if (!models.count(name))
            models[name] = model;

        const std::string flags_str = std::to_string(flags);
        if (!models.count(flags_str))
            models[flags_str] = model;

        if (!models.count(sig))
            models[sig] = model;
    }

    // TODO : Research actual compatibility flags/"versions" from libtifiles, and maybe even TI ?
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

        insertModel(-1, 0,           "Unknown", "");
        insertModel(0,  flags82,     "82",      "**TI82**");
        insertModel(1,  flags83,     "83",      "**TI83**");
        insertModel(2,  flags82a,    "82A",     "**TI83F*");
        insertModel(3,  flags84p,    "84+T",    "**TI83F*");
        insertModel(4,  flags83p,    "82+",     "**TI83F*");
        insertModel(4,  flags83p,    "83+",     "**TI83F*");
        insertModel(4,  flags84p,    "84+",     "**TI83F*");
        insertModel(5,  flags84pcse, "84+CSE",  "**TI83F*");
        insertModel(6,  flags84pce,  "84+CE",   "**TI83F*");
        insertModel(6,  flags84pce,  "84+CET",  "**TI83F*");
        insertModel(6,  flags84pcepy,"84+CETPE","**TI83F*");
        insertModel(6,  flags84pcepy,"84+CEPy", "**TI83F*");
        insertModel(7,  flags83pce,  "83PCE",   "**TI83F*");
        insertModel(7,  flags83pceep,"83PCEEP", "**TI83F*");
        insertModel(8,  flags82aep,  "82AEP",   "**TI83F*");
    }

    /**
     * @param   uint32_t    flags  The model flags
     * @return  string             The model name for those flags
     */
    std::string TIModels::getDefaultNameFromFlags(uint32_t flags)
    {
        const std::string flags_str = std::to_string(flags);
        return isValidFlags(flags) ? models[flags_str].getName() : "Unknown";
    }

    /**
     * @param   string      name   The model name
     * @return  uint32_t           The model flags for that name
     */
    uint32_t TIModels::getFlagsFromName(const std::string& name)
    {
        return isValidName(name) ? models[name].getFlags() : 0;
    }

    /**
     * @param   uint32_t    flags  The model flags
     * @return  string             The signature for those flags
     */
    std::string TIModels::getSignatureFromFlags(uint32_t flags)
    {
        const std::string flags_str = std::to_string(flags);
        return isValidFlags(flags) ? models[flags_str].getSig() : "";
    }

    /**
     * @param   string  name
     * @return  string          The signature for that name
     */
    std::string TIModels::getSignatureFromName(const std::string& name)
    {
        return isValidName(name) ? models[name].getSig() : "";
    }

    /**
     * @param   string  sig    The signature
     * @return  string          The default calc name whose file formats use that signature
     */
    std::string TIModels::getDefaultNameFromSignature(const std::string& sig)
    {
        return isValidSignature(sig) ? models[sig].getName() : "";
    }

    /**
     * @param   string  sig    The signature
     * @return  int             The default calc order ID whose file formats use that signature
     */
    int TIModels::getDefaultOrderIDFromSignature(const std::string& sig)
    {
        return isValidSignature(sig) ? models[sig].getOrderId() : -1;
    }

    /**
     * @param   string  name
     * @return  int             The default calc order ID whose file formats use that signature
     */
    int TIModels::getOrderIDFromName(const std::string& name)
    {
        return isValidName(name) ? models[name].getOrderId() : -1;
    }

    /**
     * @param   string  sig    The signature
     * @return  string          The minimum compatibility flags for that signaure
     */
    uint32_t TIModels::getMinFlagsFromSignature(const std::string& sig)
    {
        return isValidSignature(sig) ? models[sig].getFlags() : 0;
    }


    bool TIModels::isValidFlags(uint32_t flags)
    {
        const std::string flags_str = std::to_string(flags);
        return (flags != 0 && models.count(flags_str));
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