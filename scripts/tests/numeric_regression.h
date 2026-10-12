#ifndef TIVARS_NUMERIC_REGRESSION_H
#define TIVARS_NUMERIC_REGRESSION_H

#include "../../src/TypeHandlers/TypeHandlers.h"
#include <cassert>
#include <stdexcept>
#include <string>

namespace tivars::numeric_regression
{
    inline void check_fraction_roundtrip_regressions()
    {
        using namespace TypeHandlers;
        const auto check = [](const std::string& input, uint8_t type, const std::string& expected)
        {
            const options_t options = {{"_type", type}};
            const data_t before = TH_GenericReal::makeDataFromString(input, options);
            const std::string readable = TH_GenericReal::makeStringFromData(before);
            assert(readable == expected);
            assert(TH_GenericReal::makeDataFromString(readable, options) == before);
        };
        check("1001/1000", 0x18, "1001/1000");
        check("23/113", 0x18, "23/113");
        check("1/9999", 0x18, "1/9999");
        check("1/7", 0x18, "1/7");
        check("-23/113", 0x18, "-23/113");
        // A close rational that rounds to different BCD digits must be rejected.
        check("0.33333333333334", 0x18, "0.33333333333334");
        check("1.0000000000001", 0x18, "1.0000000000001");
        check("1001π/1000", 0x21, "1001π/1000");
        check("23π/113", 0x21, "23π/113");
        check("π/7", 0x21, "π/7");
        check("-23π/113", 0x21, "-23π/113");
        check("0.33333333333334π", 0x21, "0.33333333333334π");
        check("0", 0x21, "0");
        check("π", 0x21, "π");
        check("-π", 0x21, "-π");

        for (const auto* text : {"1001/1000i", "23/113i"})
        {
            const options_t options = {{"_type", 0x1B}};
            const data_t before = TH_GenericComplex::makeDataFromString(text, options);
            const std::string readable = TH_GenericComplex::makeStringFromData(before);
            assert(readable == text);
            assert(TH_GenericComplex::makeDataFromString(readable, options) == before);
        }
    }


    inline void check_fraction_input_regressions()
    {
        using namespace TypeHandlers;
        const auto rejects = [](const std::string& input, uint8_t type)
        {
            bool rejected = false;
            try { (void)TH_GenericReal::makeDataFromString(input, {{"_type", type}}); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected);
        };
        for (const auto* input : {"1x/2", "1/2foo", "1e2x/3", "1/2e2x", "1/2/3", "0x10/2", "0x1p4/2", "1/0x2", "1/inf", "nan/2", "1/-Infinity"})
            rejects(input, 0x18);
        for (const auto* input : {"1xπ/2", "π/2foo", "1e2xπ/3", "π/2e2x", "π/2/3", "0x10π/2", "0x1p4π/2", "π/0x2", "π/inf", "nanπ/2", "π/-Infinity"})
            rejects(input, 0x21);
        assert(TH_GenericReal::makeDataFromString("+1.e2/-2.5e1", {{"_type", 0x18}})
            == TH_GenericReal::makeDataFromString("-4", {{"_type", 0x18}}));
        assert(TH_GenericReal::makeDataFromString(".5π/+2E0", {{"_type", 0x21}})
            == TH_GenericReal::makeDataFromString("π/4", {{"_type", 0x21}}));
        assert(TH_GenericReal::makeDataFromString(" 1 / 2 ", {{"_type", 0x18}})
            == TH_GenericReal::makeDataFromString("1/2", {{"_type", 0x18}}));
        assert(TH_GenericReal::makeDataFromString(" π / 2 ", {{"_type", 0x21}})
            == TH_GenericReal::makeDataFromString("π/2", {{"_type", 0x21}}));
    }


    inline void check_fp_exponent_regressions()
    {
        using namespace TypeHandlers;
        const auto rejects = [](const std::string& input)
        {
            bool rejected = false;
            try { (void)STH_FP::makeDataFromString(input); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected);
        };
        assert(STH_FP::makeDataFromString("1e+99") == data_t({0x00, 0xE3, 0x10, 0, 0, 0, 0, 0, 0}));
        assert(STH_FP::makeDataFromString("1e-99") == data_t({0x00, 0x1D, 0x10, 0, 0, 0, 0, 0, 0}));
        assert(STH_FP::makeDataFromString("1e0001") == STH_FP::makeDataFromString("10"));
        // An exponent may cancel a long significand rather than exceed the range.
        assert(STH_FP::makeDataFromString("1" + std::string(100, '0') + "e-100")
            == STH_FP::makeDataFromString("1"));
        for (const auto* input : {"1e100", "1e-100", "1e2147483647", "1e-2147483647",
                                 "1e2147483648", "1e4294967296", "1e4294967297",
                                 "1e-4294967296", "1e999999999999999999999999"})
            rejects(input);
    }
}

#endif
