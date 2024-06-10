/*
 * Part of tivars_lib_cpp
 * (C) 2015-2023 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../tivarslib_utils.h"

#include <stdexcept>

#ifdef GDB_SUPPORT

#include "../json.hpp"
using json = nlohmann::ordered_json;

#include <sstream>
#include <variant>

namespace
{
    u8enum(OSColor) { Blue = 1, Red, Black, Magenta, Green, Orange, Brown, Navy, LtBlue, Yellow, White, LtGray, MedGray, Gray, DarkGray, Off };
    NLOHMANN_JSON_SERIALIZE_ENUM(OSColor, { { Blue, "Blue" }, { Red, "Red" }, { Black, "Black" }, { Magenta, "Magenta" }, { Green, "Green" }, { Orange, "Orange" },
                                            { Brown, "Brown" }, { Navy, "Navy" }, { LtBlue, "LtBlue" }, { Yellow, "Yellow" }, { White, "White" }, { LtGray, "LtGray" },
                                            { MedGray, "MedGray" }, { Gray, "Gray" }, { DarkGray, "DarkGray" }, { Off, "Off" } })

    u8enum(GlobalLineStyle) { Thick, DotThick, Thin, DotThin };
    NLOHMANN_JSON_SERIALIZE_ENUM(GlobalLineStyle, { { Thick, "Thick" }, { DotThick, "DotThick" }, { Thin, "Thin" }, { DotThin, "DotThin" } })

    u8enum(GraphMode) { Function = 16, Polar = 32, Parametric = 64, Sequence = 128 };
    NLOHMANN_JSON_SERIALIZE_ENUM(GraphMode, { { Function, "Function" }, { Polar, "Polar" }, { Parametric, "Parametric" }, { Sequence, "Sequence" } })

    u8enum(GraphStyle) { SolidLine, ThickLine, ShadeAbove, ShadeBelow, Trace, Animate, DottedLine, DotThin_ };
    NLOHMANN_JSON_SERIALIZE_ENUM(GraphStyle, { { SolidLine, "SolidLine" }, { ThickLine, "ThickLine" }, { ShadeAbove, "ShadeAbove" }, { ShadeBelow, "ShadeBelow" },
                                               { Trace, "Trace" }, { Animate, "Animate" }, { DottedLine, "DottedLine" }, { DotThin_, "Dot-Thin" } })

    struct FormatSettings
    {
        u8enum(Style) { Connected, Dot }       style      : 1 = Connected;
        u8enum(Tracing) { Sequential, Simul }  tracing    : 1 = Sequential;
        u8enum(Grid) { GridOff, GridOn }       grid       : 1 = GridOff;
        u8enum(CoordsType) { RectGC, PolarGC } coordsType : 1 = RectGC;
        u8enum(Coords) { CoordOn, CoordOff }   coords     : 1 = CoordOn;
        u8enum(Axes) { AxesOn, AxesOff }       axes       : 1 = AxesOn;
        u8enum(Label) { LabelOff, LabelOn }    label      : 1 = LabelOff;
        u8enum(GridType) { GridDot, GridLine } gridType   : 1 = GridDot;
    };
    static_assert(sizeof(FormatSettings) == 1);
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::Style, { {FormatSettings::Connected, "Connected"}, {FormatSettings::Dot, "Dot"} })
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::Tracing, { {FormatSettings::Sequential, "Sequential"}, {FormatSettings::Simul, "Simul"} })
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::Grid, { {FormatSettings::GridOff, "GridOff"}, {FormatSettings::GridOn, "GridOn"} })
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::CoordsType, { {FormatSettings::RectGC, "RectGC"}, {FormatSettings::PolarGC, "PolarGC"} })
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::Coords, { {FormatSettings::CoordOn, "CoordOn"}, {FormatSettings::CoordOff, "CoordOff"} })
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::Axes, { {FormatSettings::AxesOn, "AxesOn"}, {FormatSettings::AxesOff, "AxesOff"} })
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::Label, { {FormatSettings::LabelOff, "LabelOff"}, {FormatSettings::LabelOn, "LabelOn"} })
    NLOHMANN_JSON_SERIALIZE_ENUM(FormatSettings::GridType, { {FormatSettings::GridDot, "GridDot"}, {FormatSettings::GridLine, "GridLine"} })

    struct SeqSettings
    {
        u8enum(Mode) { Time = 0, Web = 1, WebVert = 2, SeqUV = 4, SeqVW = 8, SeqUW = 16 } mode : 5 = Time;
        uint8_t _ : 3 = 0; // We're not sure what this is, and it's also unused on the CE apparently. Sometimes it's set to 0b001.
    };
    static_assert(sizeof(SeqSettings) == 1);
    NLOHMANN_JSON_SERIALIZE_ENUM(SeqSettings::Mode, {{ SeqSettings::Time, "Time" }, { SeqSettings::Web, "Web" }, { SeqSettings::WebVert, "WebVert" }, { SeqSettings::SeqUV, "SeqUV" }, { SeqSettings::SeqVW, "SeqVW" }, { SeqSettings::SeqUW, "SeqUW" }})

    struct ExtModeSettings
    {
        u8enum(Expr) { ExprOn, ExprOff } expr : 1 = ExprOn;
        u8enum(SeqMode) { SEQ_n, SEQ_np1, SEQ_np2 } seqMode : 2 = SEQ_n;
        uint8_t _ : 5 = 0;
    };
    static_assert(sizeof(ExtModeSettings) == 1);
    NLOHMANN_JSON_SERIALIZE_ENUM(ExtModeSettings::SeqMode, {{ ExtModeSettings::SEQ_n, "SEQ(n)" }, { ExtModeSettings::SEQ_np1, "SEQ(n+1)" }, { ExtModeSettings::SEQ_np2, "SEQ(n+2)" } })

    struct FuncFlags
    {
        uint8_t unk1 : 3 = 0b011;
        uint8_t unk2 : 2 = 0;
        bool selected : 1 = false;
        bool wasUsedForGraph : 1 = false;
        bool linkTransfer : 1 = false;
    };
    static_assert(sizeof(FuncFlags) == 1);

    struct EquDefWrapper
    {
        FuncFlags flags{};
        std::string expr;
    };
    void to_json(json& j, const EquDefWrapper& fdw, const GraphStyle& graphStyle, const OSColor* color) {
        j["style"] = graphStyle;
        if (color) {
            j["color"] = *color;
        }
       j["flags"] = {
           { "selected", fdw.flags.selected },
           { "wasUsedForGraph", fdw.flags.wasUsedForGraph },
           { "linkTransfer", fdw.flags.linkTransfer }
       };
       j["expr"] = fdw.expr;
    }
    void from_json(const json& j, EquDefWrapper& fdw, GraphStyle& graphStyle, OSColor* color) {
        try { graphStyle = j["style"]; } catch (...) {}
        try { if (color && j.contains("color")) { *color = j["color"]; } } catch (...) {}
        try { fdw.flags.selected = j["flags"]["selected"]; } catch (...) {}
        try { fdw.flags.selected = j["flags"]["selected"]; } catch (...) {}
        try { fdw.flags.wasUsedForGraph = j["flags"]["wasUsedForGraph"]; } catch (...) {}
        try { fdw.flags.linkTransfer = j["flags"]["linkTransfer"]; } catch (...) {}
        try { fdw.expr = j["expr"]; } catch (...) {}
    }

    class TIReal
    {
    public:
        TIReal() = default;
        ~TIReal() = default;
        TIReal(int i) : str(std::to_string(i)) {}
        TIReal(const char* s) : str(s) {}
        TIReal(std::string s) : str(std::move(s)) {}
        operator const std::string&() const { return str; }

        std::string str; // in order to keep the precision... in the json, can be a number for convenience if precision is enough.
    };
    void to_json(json& j, const TIReal& real) {
        if (real.str.size() <= 6) { j = std::stod(real.str); } else { j = real.str; }
    }
    void from_json(const json& j, TIReal& real) {
        if (j.is_number())
        {
            std::ostringstream out;
            out.precision(15);
            out << j.get<double>();
            real = std::move(out).str();
        } else if (j.is_string()) {
            real = j.get<std::string>();
        } else {
            throw std::runtime_error("bad type, expected string or number");
        }
    }

    struct GlobalWindowSettings
    {
        TIReal Xmin = -10, Xmax = 10, Xscl = 1;
        TIReal Ymin = -10, Ymax = 10, Yscl = 1;
    };

    struct ExtSettings2
    {
        u8enum(DetectAsymptotes) { DetectAsymptotesOn, DetectAsymptotesOff } detectAsymptotes : 1 = DetectAsymptotesOn;
        uint8_t unk1 : 6 = 0;
        uint8_t unk2 : 1 = 0; // we have seen 1 here sometimes, but it doesn't appear to be used anywhere.
    };
    static_assert(sizeof(ExtSettings2) == 1);

    struct Other84CGlobalSettings
    {
        OSColor gridColor = MedGray;
        OSColor axesColor = Black;
        GlobalLineStyle globalLineStyle = Thick;
        uint8_t borderColor : 3 = 1; // 1,2,3,4 ; corresponds approximately to: LightGray, Green, LightBlue, White
        ExtSettings2 extSettings2{};
    };
    static_assert(sizeof(Other84CGlobalSettings) == 5);

#define STR_2_PI        "6.283185307"
#define STR_PI_1_24TH   "0.13089969389957"

    struct FunctionData
    {
        std::pair<const char*, TIReal> settings[1] = { {"Xres",1} };
        std::pair<const char*, GraphStyle> styles[10] = { {"Y1",{}}, {"Y2",{}}, {"Y3",{}}, {"Y4",{}}, {"Y5",{}},
                                                           {"Y6",{}}, {"Y7",{}}, {"Y8",{}}, {"Y9",{}}, {"Y0",{}} };
        std::pair<const char*, EquDefWrapper> equations[10] = { {"Y1",{}}, {"Y2",{}}, {"Y3",{}}, {"Y4",{}}, {"Y5",{}},
                                                                 {"Y6",{}}, {"Y7",{}}, {"Y8",{}}, {"Y9",{}}, {"Y0",{}} };
        std::pair<const char*, OSColor> colors[10] = { {"Y1",Blue}, {"Y2",Red}, {"Y3",Black}, {"Y4",Magenta}, {"Y5",Green},
                                                       {"Y6",Orange}, {"Y7",Brown}, {"Y8",Blue}, {"Y9",Red}, {"Y0",Black} };
    };

    struct ParametricData
    {
        std::pair<const char*, TIReal> settings[3] = { {"Tmin",0}, {"Tmax",STR_2_PI}, {"Tstep",STR_PI_1_24TH} };
        std::pair<const char*, GraphStyle> styles[6] = { {"X1T_Y1T",{}}, {"X2T_Y2T",{}}, {"X3T_Y3T",{}},
                                                         {"X4T_Y4T",{}}, {"X5T_Y5T",{}}, {"X6T_Y6T",{}} };
        std::pair<const char*, EquDefWrapper> equations[12] = { {"X1T",{}}, {"Y1T",{}}, {"X2T",{}}, {"Y2T",{}}, {"X3T",{}}, {"Y3T",{}},
                                                                 {"X4T",{}}, {"Y4T",{}}, {"X5T",{}}, {"Y5T",{}}, {"X6T",{}}, {"Y6T",{}} };
        std::pair<const char*, OSColor> colors[6] = { {"X1T_Y1T",Blue}, {"X2T_Y2T",Red}, {"X3T_Y3T",Black},
                                                      {"X4T_Y4T",Magenta}, {"X5T_Y5T",Green}, {"X6T_Y6T",Orange} };
    };

    struct PolarData
    {
        std::pair<const char*, TIReal> settings[3] = { {"THmin",{}}, {"THmax",STR_2_PI}, {"THstep",STR_PI_1_24TH} };
        std::pair<const char*, GraphStyle> styles[6] = { {"r1",{}}, {"r2",{}}, {"r3",{}}, {"r4",{}}, {"r5",{}}, {"r6",{}} };
        std::pair<const char*, EquDefWrapper> equations[6] = { {"r1",{}}, {"r2",{}}, {"r3",{}}, {"r4",{}}, {"r5",{}}, {"r6",{}} };
        std::pair<const char*, OSColor> colors[6] = { {"r1",Blue}, {"r2",Red}, {"r3",Black}, {"r4",Magenta}, {"r5",Green}, {"r6",Orange} };
    };

    struct SequenceData
    {
        std::pair<const char*, TIReal> settings[10] = { {"PlotStart",1}, {"nMax",10}, {"u(nMin)",0}, {"v(nMin)",0}, {"nMin",1},
                                                        {"u(nMin+1)",0}, {"v(nMin+1)",0}, {"w(nMin+1)",0}, {"PlotStep",1}, {"w(nMin)",0}};
        std::pair<const char*, GraphStyle> styles[3] = { {"u",{}}, {"v",{}}, {"w",{}} };
        std::pair<const char*, EquDefWrapper> equations[3] = { {"u",{}}, {"v",{}}, {"w",{}} };
        std::pair<const char*, OSColor> colors[3] = { {"u",Blue}, {"v",Red}, {"w",Black} };
    };

    struct GDB
    {
        bool _has84CAndLaterData = false;
        GraphMode graphMode{};
        FormatSettings formatSettings{};
        SeqSettings seqSettings{};
        ExtModeSettings extSettings{};
        GlobalWindowSettings globalWindowSettings{};
        std::variant<FunctionData,ParametricData,PolarData,SequenceData> specificData;
        Other84CGlobalSettings global84CSettings{};
    };

    void from_json(const json& j, GDB& gdb)
    {
        gdb._has84CAndLaterData = j.contains("global84CSettings") && j["global84CSettings"].is_object();

        try { gdb.graphMode = j["graphMode"]; } catch(...) {}

        if (j.contains("formatSettings"))
        {
            if (!j["formatSettings"].is_array()) {
                throw std::runtime_error("bad type for formatSettings, expected array");
            }
            const size_t size = j["formatSettings"].size();
            if (size >= 1) try { gdb.formatSettings.style      = j["formatSettings"][0]; } catch(...) {}
            if (size >= 2) try { gdb.formatSettings.tracing    = j["formatSettings"][1]; } catch(...) {}
            if (size >= 3) try { gdb.formatSettings.grid       = j["formatSettings"][2]; } catch(...) {}
            if (size >= 4) try { gdb.formatSettings.coordsType = j["formatSettings"][3]; } catch(...) {}
            if (size >= 5) try { gdb.formatSettings.coords     = j["formatSettings"][4]; } catch(...) {}
            if (size >= 6) try { gdb.formatSettings.axes       = j["formatSettings"][5]; } catch(...) {}
            if (size >= 7) try { gdb.formatSettings.label      = j["formatSettings"][6]; } catch(...) {}
            if (size >= 8) try { gdb.formatSettings.gridType   = j["formatSettings"][7]; } catch(...) {}
        }

        if (j.contains("seqSettings"))
        {
            if (!j["seqSettings"].is_object()) {
                throw std::runtime_error("bad type for seqSettings, expected object");
            }
            try { gdb.seqSettings.mode = j["seqSettings"]["mode"]; } catch(...) {}
        }

        if (j.contains("extSettings"))
        {
            if (!j["extSettings"].is_object()) {
                throw std::runtime_error("bad type for extSettings, expected object");
            }
            const auto& es = j["globalWindowSettings"];
            if (es.contains("showExpr")) try { gdb.extSettings.expr = es["showExpr"].get<bool>() ? ExtModeSettings::ExprOn : ExtModeSettings::ExprOff; } catch(...) {}
            if (es.contains("seqMode")) try { gdb.extSettings.seqMode = es["seqMode"]; } catch(...) {}
        }

        if (j.contains("globalWindowSettings"))
        {
            if (!j["globalWindowSettings"].is_object()) {
                throw std::runtime_error("bad type for globalWindowSettings, expected object");
            }
            const auto& gws = j["globalWindowSettings"];
            if (gws.contains("Xmin")) try { from_json(gws["Xmin"], gdb.globalWindowSettings.Xmin); } catch(...) {}
            if (gws.contains("Xmax")) try { from_json(gws["Xmax"], gdb.globalWindowSettings.Xmax); } catch(...) {}
            if (gws.contains("Xscl")) try { from_json(gws["Xscl"], gdb.globalWindowSettings.Xscl); } catch(...) {}
            if (gws.contains("Ymin")) try { from_json(gws["Ymin"], gdb.globalWindowSettings.Ymin); } catch(...) {}
            if (gws.contains("Ymax")) try { from_json(gws["Ymax"], gdb.globalWindowSettings.Ymax); } catch(...) {}
            if (gws.contains("Yscl")) try { from_json(gws["Yscl"], gdb.globalWindowSettings.Yscl); } catch(...) {}
        }

        const auto getSpecificDataFromJSON = [&](const json& j, const GraphMode& graphMode, auto& specificData) -> void
        {
            for (auto& [name, value] : specificData.settings)
            {
                if (j["specificData"].contains("settings") && j["specificData"]["settings"].contains(name))
                {
                    from_json(j["specificData"]["settings"][name], value);
                }
            }

            if (j["specificData"].contains("equations"))
            {
                if (!j["specificData"]["equations"].is_object())
                {
                    throw std::runtime_error("need specificData.equations to be an object");
                }

                uint8_t i = 0;
                for (auto& [name, equDefWrapper] : specificData.equations)
                {
                    if (j["specificData"]["equations"].contains(name))
                    {
                        auto& value = j["specificData"]["equations"][name];
                        if (value.is_object())
                        {
                            const uint8_t idx = graphMode == Parametric ? (uint8_t)(i/2.0) : i; // Parametric is split in 2 eqs
                            from_json(value, equDefWrapper, specificData.styles[idx].second, gdb._has84CAndLaterData ? &specificData.colors[idx].second : nullptr);
                        }
                    }
                    i++;
                }
            }

        };

        switch (gdb.graphMode) {
            case Function:   gdb.specificData = FunctionData{};   getSpecificDataFromJSON(j, gdb.graphMode, std::get<FunctionData>(gdb.specificData));   break;
            case Parametric: gdb.specificData = ParametricData{}; getSpecificDataFromJSON(j, gdb.graphMode, std::get<ParametricData>(gdb.specificData)); break;
            case Polar:      gdb.specificData = PolarData{};      getSpecificDataFromJSON(j, gdb.graphMode, std::get<PolarData>(gdb.specificData));      break;
            case Sequence:   gdb.specificData = SequenceData{};   getSpecificDataFromJSON(j, gdb.graphMode, std::get<SequenceData>(gdb.specificData));   break;
            default:
                throw std::runtime_error("Unknown graphMode value " + std::to_string(gdb.graphMode));
        }

        if (gdb._has84CAndLaterData && j["global84CSettings"].is_object())
        {
            const auto& g84cs = j["global84CSettings"];
            if (g84cs.contains("colors"))
            {
                if (g84cs["colors"].contains("grid")) try { gdb.global84CSettings.gridColor = g84cs["colors"]["grid"]; } catch (...) {}
                if (g84cs["colors"].contains("axes")) try { gdb.global84CSettings.axesColor = g84cs["colors"]["axes"]; } catch (...) {}
                if (g84cs["colors"].contains("border")) try { gdb.global84CSettings.borderColor = g84cs["colors"]["border"]; } catch (...) {}
            }
            if (g84cs.contains("other"))
            {
                if (g84cs["other"].contains("globalLineStyle")) try { gdb.global84CSettings.globalLineStyle = g84cs["other"]["globalLineStyle"]; } catch (...) {}
                if (g84cs["other"].contains("detectAsymptotes")) try { gdb.global84CSettings.extSettings2.detectAsymptotes = g84cs["other"]["detectAsymptotes"].get<bool>() ? ExtSettings2::DetectAsymptotesOn : ExtSettings2::DetectAsymptotesOff; } catch(...) {}
            }
        }
    }

    void to_json(json& j, const GDB& gdb) {
        j = json{
            { "graphMode", gdb.graphMode },
            { "formatSettings", {
                gdb.formatSettings.style,
                gdb.formatSettings.tracing,
                gdb.formatSettings.grid,
                gdb.formatSettings.coordsType,
                gdb.formatSettings.coords,
                gdb.formatSettings.axes,
                gdb.formatSettings.label,
                gdb.formatSettings.gridType
            } },
            { "seqSettings", {
                { "mode", gdb.seqSettings.mode }
            } },
            { "extSettings", {
                { "showExpr", !((bool)gdb.extSettings.expr) },
                { "seqMode", gdb.extSettings.seqMode },
            } }
        };

        to_json(j["globalWindowSettings"]["Xmin"], gdb.globalWindowSettings.Xmin);
        to_json(j["globalWindowSettings"]["Xmax"], gdb.globalWindowSettings.Xmax);
        to_json(j["globalWindowSettings"]["Xscl"], gdb.globalWindowSettings.Xscl);
        to_json(j["globalWindowSettings"]["Ymin"], gdb.globalWindowSettings.Ymin);
        to_json(j["globalWindowSettings"]["Ymax"], gdb.globalWindowSettings.Ymax);
        to_json(j["globalWindowSettings"]["Yscl"], gdb.globalWindowSettings.Yscl);

        std::visit([&](auto&& specificData)
        {
           for (const auto& [name, value] : specificData.settings)
           {
               to_json(j["specificData"]["settings"][name], value);
           }

           uint8_t i = 0;
           for (const auto& [name, equDefWrapper] : specificData.equations)
           {
               const uint8_t idx = (std::is_same_v<std::decay_t<decltype(specificData)>, ParametricData>) ? (uint8_t)(i/2.) : i;
               const OSColor* oscolorPtr = gdb._has84CAndLaterData ? &specificData.colors[idx].second : nullptr;
               to_json(j["specificData"]["equations"][name], equDefWrapper, specificData.styles[idx].second, oscolorPtr);
               i++;
           }
        }, gdb.specificData);

        if (gdb._has84CAndLaterData)
        {
            j["global84CSettings"] = {
                { "colors", {
                    { "grid", gdb.global84CSettings.gridColor },
                    { "axes", gdb.global84CSettings.axesColor },
                    { "border", gdb.global84CSettings.borderColor },
                } },
                { "other", {
                    { "globalLineStyle", gdb.global84CSettings.globalLineStyle },
                    { "detectAsymptotes", gdb.global84CSettings.extSettings2.detectAsymptotes == ExtSettings2::DetectAsymptotesOn }
                } },
            };
        }
    }
}

namespace tivars
{
    data_t TH_GDB::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;

        GDB gdb{};
        from_json(json::parse(str), gdb);

        data_t data(2); // reserve 2 bytes for size fields, filled later

        data.push_back(0); // always 0
        data.push_back(gdb.graphMode);
        data.push_back(*((uint8_t*)(&gdb.formatSettings)));
        data.push_back(*((uint8_t*)(&gdb.seqSettings)));
        data.push_back(*((uint8_t*)(&gdb.extSettings)));

        vector_append(data, STH_FP::makeDataFromString(gdb.globalWindowSettings.Xmin));
        vector_append(data, STH_FP::makeDataFromString(gdb.globalWindowSettings.Xmax));
        vector_append(data, STH_FP::makeDataFromString(gdb.globalWindowSettings.Xscl));
        vector_append(data, STH_FP::makeDataFromString(gdb.globalWindowSettings.Ymin));
        vector_append(data, STH_FP::makeDataFromString(gdb.globalWindowSettings.Ymax));
        vector_append(data, STH_FP::makeDataFromString(gdb.globalWindowSettings.Yscl));

        std::visit([&](auto&& specificData)
        {
           for (const auto& [_, value] : specificData.settings)
           {
               vector_append(data, STH_FP::makeDataFromString(value));
           }

           for (const auto& [_, value] : specificData.styles)
           {
               data.push_back(value);
           }

           for (const auto& [_, equ] : specificData.equations)
           {
               data.push_back(*((uint8_t*)(&equ.flags)));
               vector_append(data, TH_Tokenized::makeDataFromString(equ.expr));
           }

           if (gdb._has84CAndLaterData)
           {
               vector_append(data, data_t(magic84CAndLaterSectionMarker, magic84CAndLaterSectionMarker + strlen(magic84CAndLaterSectionMarker)));
               for (const auto& [_, value] : specificData.colors)
               {
                   data.push_back(value);
               }
           }
        }, gdb.specificData);

        if (gdb._has84CAndLaterData)
        {
            data.push_back(gdb.global84CSettings.gridColor);
            data.push_back(gdb.global84CSettings.axesColor);
            data.push_back(gdb.global84CSettings.globalLineStyle);
            data.push_back(gdb.global84CSettings.borderColor);
            data.push_back(*((uint8_t*)(&gdb.global84CSettings.extSettings2)));
        }

        const size_t length = data.size() - 2;
        data[0] = (uint8_t)(length & 0xFF);
        data[1] = (uint8_t)((length >> 8) & 0xFF);

        return data;
    }

    std::string TH_GDB::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        const size_t dataSizeActual = data.size();
        if (dataSizeActual < dataByteCountMinimum)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least " + std::to_string(dataByteCountMinimum) + " bytes");
        }

        const size_t dataSizeExpected = (data[0] & 0xFF) + ((data[1] & 0xFF) << 8);
        if (dataSizeActual - 2 != dataSizeExpected)
        {
            throw std::invalid_argument("Invalid data array. dataSizeActual-2 (" + std::to_string(dataSizeActual-2) + ") != dataSizeExpected (" + std::to_string(dataSizeExpected) + ")");
        }

        GDB gdb = {
            .graphMode = static_cast<GraphMode>(data[3]),
            .formatSettings = *((FormatSettings*)(&data[4])),
            .seqSettings = *((SeqSettings*)(&data[5])),
            .extSettings = *((ExtModeSettings*)(&data[6])),
            .globalWindowSettings = {
                .Xmin = STH_FP::makeStringFromData(data_t(data.begin()+7 + 0*STH_FP::dataByteCount, data.begin()+7 + 1*STH_FP::dataByteCount)),
                .Xmax = STH_FP::makeStringFromData(data_t(data.begin()+7 + 1*STH_FP::dataByteCount, data.begin()+7 + 2*STH_FP::dataByteCount)),
                .Xscl = STH_FP::makeStringFromData(data_t(data.begin()+7 + 2*STH_FP::dataByteCount, data.begin()+7 + 3*STH_FP::dataByteCount)),
                .Ymin = STH_FP::makeStringFromData(data_t(data.begin()+7 + 3*STH_FP::dataByteCount, data.begin()+7 + 4*STH_FP::dataByteCount)),
                .Ymax = STH_FP::makeStringFromData(data_t(data.begin()+7 + 4*STH_FP::dataByteCount, data.begin()+7 + 5*STH_FP::dataByteCount)),
                .Yscl = STH_FP::makeStringFromData(data_t(data.begin()+7 + 5*STH_FP::dataByteCount, data.begin()+7 + 6*STH_FP::dataByteCount))
            }
        };

        off_t tmpOff = 7 + 6*STH_FP::dataByteCount;

        const auto parseSpecificData = [&](auto& specificData) -> void
        {
            for (auto& [_, settingValue] : specificData.settings)
            {
                settingValue = STH_FP::makeStringFromData(data_t(data.begin() + tmpOff, data.begin() + tmpOff + STH_FP::dataByteCount));
                tmpOff += STH_FP::dataByteCount;
            }

            for (auto& [_, style] : specificData.styles)
            {
                style = static_cast<GraphStyle>(data[tmpOff]);
                tmpOff++;
            }

            for (auto& [_, equ] : specificData.equations)
            {
                const uint16_t equLen = (data[tmpOff + 2] << 1) + data[tmpOff + 1];
                equ.flags = *(FuncFlags*)(&(data[tmpOff]));
                equ.expr = TH_Tokenized::makeStringFromData(data_t(data.begin() + tmpOff + 3, data.begin() + tmpOff + 3 + equLen), {{"fromRawBytes", true}});
                tmpOff += sizeof(equLen) + sizeof(equ.flags) + equLen;
            }

            if (std::memcmp(&data[tmpOff], magic84CAndLaterSectionMarker, strlen(magic84CAndLaterSectionMarker)) == 0)
            {
                tmpOff += strlen(magic84CAndLaterSectionMarker);
                gdb._has84CAndLaterData = true;
                for (auto& [_, color] : specificData.colors)
                {
                    color = static_cast<OSColor>(data[tmpOff]);
                    tmpOff++;
                }
            }
        };

        switch (gdb.graphMode) {
            case Function:   gdb.specificData = FunctionData{};   parseSpecificData(std::get<FunctionData>(gdb.specificData));   break;
            case Parametric: gdb.specificData = ParametricData{}; parseSpecificData(std::get<ParametricData>(gdb.specificData)); break;
            case Polar:      gdb.specificData = PolarData{};      parseSpecificData(std::get<PolarData>(gdb.specificData));      break;
            case Sequence:   gdb.specificData = SequenceData{};   parseSpecificData(std::get<SequenceData>(gdb.specificData));   break;
            default:
                throw std::runtime_error("Unknown graphMode value " + std::to_string(gdb.graphMode));
        }

        if (gdb._has84CAndLaterData)
        {
            gdb.global84CSettings.gridColor = static_cast<OSColor>(data[tmpOff++]);
            gdb.global84CSettings.axesColor = static_cast<OSColor>(data[tmpOff++]);
            gdb.global84CSettings.globalLineStyle = static_cast<GlobalLineStyle>(data[tmpOff++]);
            gdb.global84CSettings.borderColor = data[tmpOff++];
            gdb.global84CSettings.extSettings2 = *((ExtSettings2*)(&data[tmpOff++]));
        }

        const bool compactJSON = has_option(options, "compact") && options.at("compact") == 1;
        return json(gdb).dump(compactJSON ? -1 : 4);
    }
}

#else

namespace tivars
{
    data_t TH_GDB::makeDataFromString(const std::string&, const options_t&, const TIVarFile* _ctx)
    {
        throw std::runtime_error("GDB support is not compiled in this tivars_lib_cpp version");
    }

    std::string TH_GDB::makeStringFromData(const data_t&, const options_t&, const TIVarFile* _ctx)
    {
        throw std::runtime_error("GDB support is not compiled in this tivars_lib_cpp version");
    }
}

#endif /* GDB_SUPPORT */
