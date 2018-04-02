/*
 * Part of tivars_lib_cpp
 * (C) 2015-2018 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../utils.h"

#include <regex>
#include <unordered_map>

using namespace std;

namespace tivars
{
    static const unordered_map<uchar, handler_pair_t> type2handlers = {
        { 0x0C, make_handler_pair(STH_FP)              },
        { 0x1B, make_handler_pair(STH_ExactFraction)   },
        { 0x1D, make_handler_pair(STH_ExactRadical)    },
        { 0x1E, make_handler_pair(STH_ExactPi)         },
        { 0x1F, make_handler_pair(STH_ExactFractionPi) },
    };
    static const unordered_map<uchar, string> type2patterns = {
        { 0x0C, STH_FP::validPattern              },
        { 0x1B, STH_ExactFraction::validPattern   },
        { 0x1D, STH_ExactRadical::validPattern    },
        { 0x1E, STH_ExactPi::validPattern         },
        { 0x1F, STH_ExactFractionPi::validPattern },
    };

    static bool checkValidStringAndGetMatches(const string& str, const string& typePattern, smatch& matches)
    {
        if (str.empty())
        {
            return false;
        }
        // Handle real only, real+imag, imag only.
        bool isValid = regex_match(str, matches, regex("^"   + typePattern + "()$"))
                    || regex_match(str, matches, regex("^"   + typePattern + typePattern + "i$"))
                    || regex_match(str, matches, regex("^()" + typePattern + "i$"));
        return isValid;
    }

    // For this, we're going to assume that both members are of the same type...
    // TODO: guess the type instead of reading it from the options
    data_t TH_GenericComplex::makeDataFromString(const string& str, const options_t& options)
    {
        const size_t bytesPerMember = 9;
        const auto& typeIter = options.find("_type");
        if (typeIter == options.end())
        {
            throw std::runtime_error("Needs _type in options for TH_GenericComplex::makeDataFromString");
        }
        const uchar type = (uchar)typeIter->second;
        const auto& handlerIter = type2handlers.find(type);
        if (handlerIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericComplex: " + to_string(type));
        }
        const auto& handler = handlerIter->second.first;

        string newStr = str;
        newStr = regex_replace(newStr, regex(" "), "");
        newStr = regex_replace(newStr, regex("\\+i"), "+1i");
        newStr = regex_replace(newStr, regex("-i"), "-1i");

        smatch matches;
        bool isValid = checkValidStringAndGetMatches(newStr, type2patterns.at(type), matches);
        if (!isValid || matches.size() != 3)
        {
            throw invalid_argument("Invalid input string. Needs to be a valid complex subtype string");
        }

        data_t data;
        for (int i=0; i<2; i++)
        {
            string coeff = matches[i+1];
            if (coeff.empty())
            {
                coeff = "0";
            }

            const data_t& member_data = handler(coeff, options);
            data.insert(data.end(), member_data.begin(), member_data.end());

            uchar flags = 0;
            flags |= (atof(coeff.c_str()) < 0) ? (1 << 7) : 0;
            flags |= (1 << 2); // Because it's a complex number
            flags |= (1 << 3); // Because it's a complex number

            data[i * bytesPerMember] = flags;
        }

        return data;
    }

    string TH_GenericComplex::makeStringFromData(const data_t& data, const options_t& options)
    {
        const size_t bytesPerMember = 9;

        if (data.size() != 2*bytesPerMember)
        {
            throw invalid_argument("Invalid data array. Needs to contain 18 bytes");
        }

        // 0x1F because we discard the flags bit (see above)
        const uchar typeR = (uchar)(data[0] & 0x1F);
        const uchar typeI = (uchar)(data[bytesPerMember] & 0x1F);

        const auto& handlerRIter = type2handlers.find(typeR);
        if (handlerRIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericComplex: " + to_string(typeR));
        }
        const auto& handlerIIter = type2handlers.find(typeI);
        if (handlerIIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericComplex: " + to_string(typeI));
        }

        const auto& handlerR = handlerRIter->second.second;
        const auto& handlerI = handlerIIter->second.second;

        const data_t::const_iterator mid = data.cbegin() + bytesPerMember;
        string coeffR = handlerR(data_t(data.cbegin(), mid), options);
        string coeffI = handlerI(data_t(mid, data.cend()), options);

        const bool coeffRZero = coeffR == "0";
        const bool coeffIZero = coeffI == "0";
        string str;
        str.reserve(coeffR.length() + 1 + coeffI.length() + 1);
        if (!coeffRZero || coeffIZero) {
            str += coeffR;
        }
        if (!coeffRZero && !coeffIZero && coeffI.front() != '-' && coeffI.front() != '+') {
            str += '+';
        }
        if (!coeffIZero) {
            if (coeffI != "1") {
                str += coeffI;
            }
            str += 'i';
        }
        return str;
    }

}
