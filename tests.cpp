/*
 * Part of tivars_lib_cpp
 * (C) 2015-2019 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include <cassert>
#include <cstring>
#include <string>
#include <iostream>

#include "src/TIModels.h"
#include "src/TIVarTypes.h"
#include "src/BinaryFile.h"
#include "src/TIVarFile.h"
#include "src/TypeHandlers/TypeHandlers.h"
#include "src/tivarslib_utils.h"

using namespace std;
using namespace tivars;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /* Init Stuff */

    TIModels::initTIModelsArray();
    TIVarTypes::initTIVarTypesArray();
    TH_Tokenized::initTokens();


    using tivars::TH_Tokenized::LANG_FR;
    using tivars::TH_Tokenized::LANG_EN;

    /* Tests */

    assert(TIVarTypes::getIDFromName("ExactRealPi") == 32);

    {
        TIVarFile toksPrgm = TIVarFile::loadFromFile("testData/ALLTOKS.8Xp");
        cout << toksPrgm.getReadableContent() << "\n" << endl;
    }

    {
        TIVarFile testAppVar = TIVarFile::createNew(TIVarType::createFromName("AppVar"), "TEST");
        testAppVar.setContentFromString("ABCD1234C9C8C7C6"); // random but valid hex string
        assert(testAppVar.getReadableContent() == "ABCD1234C9C8C7C6");
        assert(testAppVar.getRawContent().size() == strlen("ABCD1234C9C8C7C6") / 2 + 2);
        testAppVar.saveVarToFile("testData", "testAVnew");
    }

    {
        TIVarFile testString = TIVarFile::loadFromFile("testData/String.8xs");
        assert(testString.getReadableContent() == "Hello World");
    }

    {
        TIVarFile testPrgmQuotes = TIVarFile::loadFromFile("testData/testPrgmQuotes.8xp");
        cout << "testPrgmQuotes.getReadableContent() : " << testPrgmQuotes.getReadableContent() << endl;
        assert(testPrgmQuotes.getReadableContent() == "Pause \"2 SECS\",2");
    }

    {
        TIVarFile testEquation = TIVarFile::loadFromFile("testData/Equation_Y1T.8xy");
        assert(testEquation.getReadableContent() == "3sin(T)+4");
    }

    {
        TIVarFile testReal = TIVarFile::loadFromFile("testData/Real.8xn");
        assert(testReal.getRawContent() == data_t({0x80,0x81,0x42,0x13,0x37,0x00,0x00,0x00,0x00}));
        assert(testReal.getReadableContent() == "-42.1337");
        testReal.setContentFromString("5");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x80,0x50,0x00,0x00,0x00,0x00,0x00,0x00}));
        assert(testReal.getReadableContent() == "5");
        testReal.setContentFromString(".5");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x7F,0x50,0x00,0x00,0x00,0x00,0x00,0x00}));
        assert(testReal.getReadableContent() == "0.5");
        testReal.setContentFromString(".000000999999999999999e105");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x80+99,0x10,0x00,0x00,0x00,0x00,0x00,0x00}));
        assert(testReal.getReadableContent() == "1e99");
        testReal.setContentFromString("3.14159265358979323846264");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x80   ,0x31,0x41,0x59,0x26,0x53,0x58,0x98}));
        assert(testReal.getReadableContent() == "3.1415926535898");
        testReal.setContentFromString("-1234567890123456789e+12");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x80,0x80+30,0x12,0x34,0x56,0x78,0x90,0x12,0x35}));
        assert(testReal.getReadableContent() == "-1.2345678901235e30");
    }

#ifndef __EMSCRIPTEN__
    {
        try
        {
            data_t bad_real = data_t({1, 0, 0, 0, 0, 0, 0, 0, 0});
            TH_GenericReal::makeStringFromData(bad_real);
            assert(false);
        }
        catch (exception& e)
        {
            cout << "Caught expected exception: " << e.what() << endl;
        }
    }
#endif

    {
        TIVarFile testReal42 = TIVarFile::createNew(TIVarType::createFromName("Real"), "R");
        testReal42.setCalcModel(TIModel::createFromName("84+"));
        testReal42.setContentFromString("9001.42");
        cout << "testReal42.getReadableContent() : " << testReal42.getReadableContent() << endl;
        assert(testReal42.getReadableContent() == "9001.42");
        testReal42.setContentFromString("-0.00000008");
        cout << "testReal42.getReadableContent() : " << testReal42.getReadableContent() << endl;
        assert(atof(testReal42.getReadableContent().c_str()) == -8e-08);
        testReal42.saveVarToFile("testData", "Real_new");
    }

    {
        string test = "Disp 42:Wait 5:toString(42):Pause\nInput A,\"?\":Asdf(123)\nFor(I,1,10)\nThen\nDisp I:For(J,1,10)\nThen\nDisp J\nEnd\nEnd";
        cout << "Indented code:" << endl << TH_Tokenized::reindentCodeString(test) << endl;
    }

    {
        TIVarFile testPrgmReindent = TIVarFile::createNew(TIVarType::createFromName("Program"), "asdf");
        testPrgmReindent.setContentFromString("\"http://TIPlanet.org");
        assert(trim(testPrgmReindent.getReadableContent({{"prettify", true}, {"reindent", true}})) == "\"http://TIPlanet.org");
    }

#ifndef __EMSCRIPTEN__
    {
        try
        {
            auto goodTypeForCalc = TIVarFile::createNew(TIVarType::createFromName("Program"), "Bla", TIModel::createFromName("83PCE"));
        } catch (exception& e) {
            cout << "Caught unexpected exception: " << e.what() << endl;
            assert(false);
        }
        try
        {
            auto badTypeForCalc = TIVarFile::createNew(TIVarType::createFromName("ExactComplexFrac"), "Bla", TIModel::createFromName("84+"));
            assert(false);
        } catch (exception& e) {
            cout << "Caught expected exception: " << e.what() << endl;
        }
    }
#endif

    assert(TIVarTypes::getIDFromName("ExactRealPi") == 32);

    {
        TIVarFile testPrgm = TIVarFile::loadFromFile("testData/Program.8xp");
        cout << "testPrgm.getHeader().entries_len = " << testPrgm.getHeader().entries_len
             << "\t testPrgm.size() - 57 == " << (testPrgm.size() - 57) << endl;
        assert(testPrgm.getHeader().entries_len == testPrgm.size() - 57);
        string testPrgmcontent = testPrgm.getReadableContent({{"lang", LANG_FR}});

        TIVarFile newPrgm = TIVarFile::createNew(TIVarType::createFromName("Program"));
        newPrgm.setContentFromString(testPrgmcontent);
        string newPrgmcontent = newPrgm.getReadableContent({{"lang", LANG_FR}});

        assert(testPrgmcontent == newPrgmcontent);
        newPrgm.saveVarToFile("testData", "Program_new");

        cout << endl << "testPrgmcontent : " << endl << testPrgmcontent << endl;
        cout << "\n\n\n" << endl;
    }

    {
        TIVarFile testPrgm42 = TIVarFile::createNew(TIVarType::createFromName("Program"), "asdf");
        testPrgm42.setCalcModel(TIModel::createFromName("82A"));
        testPrgm42.setContentFromString("Grande blabla:Disp \"Grande blabla");
        testPrgm42.setVarName("MyProgrm");
        assert(testPrgm42.getReadableContent() == "Grande blabla:Disp \"Grande blabla");
        testPrgm42.saveVarToFile("testData", "testMinTok_new");
        testPrgm42.setArchived(true);
        testPrgm42.saveVarToFile("testData", "testMinTok_archived_new");
    }

    {
        TIVarFile testPrgm = TIVarFile::createNew(TIVarType::createFromName("Program"), "asdf");
        testPrgm.setContentFromString("Pause 42:Pause 43:Disp \"\",\"Bouh la =/*: déf\",\"suite :\",\" OK");
        string testPrgmcontent = testPrgm.getReadableContent({{"prettify", true},  {"reindent", true}});
        assert(trim(testPrgmcontent) == "Pause 42\nPause 43\nDisp \"\",\"Bouh la =/*: déf\",\"suite :\",\" OK");
    }

    {
        TIVarFile testRealList = TIVarFile::loadFromFile("testData/RealList.8xl");
        cout << "Before: " << testRealList.getReadableContent() << "\n   Now: ";
        testRealList.setContentFromString("{9, 0, .5, -6e-8}");
        cout << testRealList.getReadableContent() << "\n";
        testRealList.saveVarToFile("testData", "RealList_new");
    }

    {
        TIVarFile testStandardMatrix = TIVarFile::loadFromFile("testData/Matrix_3x3_standard.8xm");
        cout << "Before: " << testStandardMatrix.getReadableContent() << "\n   Now: ";
        testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7,-8,-9]]");
        testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7.5,-8,-9][1,2,3][4,5,6][-0.002,-8,-9]]");
        cout << testStandardMatrix.getReadableContent() << "\n";
        testStandardMatrix.saveVarToFile("testData", "Matrix_new");
    }

    {
        TIVarFile testComplex = TIVarFile::loadFromFile("testData/Complex.8xc"); // -5 + 2i
        cout << "Before: " << testComplex.getReadableContent() << "\n   Now: ";
        assert(testComplex.getReadableContent() == "-5+2i");
        TIVarFile newComplex = TIVarFile::createNew(TIVarType::createFromName("Complex"), "C");
        newComplex.setContentFromString("-5+2i");
        assert(newComplex.getRawContent() == newComplex.getRawContent());
        newComplex.setContentFromString("2.5+0.001i");
        cout << "After: " << newComplex.getReadableContent() << endl;
        testComplex.saveVarToFile("testData", "Complex_new");
    }

    {
        TIVarFile testComplexList = TIVarFile::loadFromFile("testData/ComplexList.8xl");
        cout << "Before: " << testComplexList.getReadableContent() << "\n   Now: ";
        testComplexList.setContentFromString("{9+2i, 0i, .5, -0.5+6e-8i}");
        cout << testComplexList.getReadableContent() << "\n";
        testComplexList.saveVarToFile("testData", "ComplexList_new");
    }

    {
        TIVarFile testExact_RealRadical = TIVarFile::loadFromFile("testData/Exact_RealRadical.8xn");
        cout << "Before: " << testExact_RealRadical.getReadableContent() << endl;
        assert(testExact_RealRadical.getReadableContent() == "(41*√(789)+14*√(654))/259");
        //TIVarFile newExact_RealRadical = TIVarFile::createNew(TIVarType::createFromName("ExactRealRadical"), "A", TIModel::createFromName("83PCE"));
        //newExact_RealRadical.setContentFromString("-42.1337");
        //assert(testExact_RealRadical.getRawContent() == newExact_RealRadical.getRawContent());
        //newExact_RealRadical.saveVarToFile("testData", "Exact_RealRadical_new");
    }

    {
        TIVarFile testExactComplexFrac = TIVarFile::loadFromFile("testData/Exact_ComplexFrac.8xc");
        cout << "Before: " << testExactComplexFrac.getReadableContent() << endl;
        assert(testExactComplexFrac.getReadableContent() == "1/5-2/5i");
        //TIVarFile newExactComplexFrac = TIVarFile::createNew(TIVarType::createFromName("ExactComplexFrac"), "A", TIModel::createFromName("83PCE"));
        //newExactComplexFrac.setContentFromString("-42.1337");
        //assert(testExactComplexFrac.getRawContent() == newExactComplexFrac.getRawContent());
        //newExactComplexFrac.saveVarToFile("testData", "Exact_ComplexFrac_new");
    }


    {
        TIVarFile testExactComplexPi = TIVarFile::loadFromFile("testData/Exact_ComplexPi.8xc");
        cout << "Before: " << testExactComplexPi.getReadableContent() << endl;
        assert(testExactComplexPi.getReadableContent() == "1/5-3πi");
        //TIVarFile newExactComplexPi = TIVarFile::createNew(TIVarType::createFromName("ExactComplexPi"), "A", TIModel::createFromName("83PCE"));
        //newExactComplexPi.setContentFromString("-42.1337");
        //assert(testExactComplexPi.getRawContent() == newExactComplexPi.getRawContent());
        //newExactComplexPi.saveVarToFile("testData", "Exact_ComplexPi_new");
    }

    {
        TIVarFile testExactComplexPiFrac = TIVarFile::loadFromFile("testData/Exact_ComplexPiFrac.8xc");
        cout << "Before: " << testExactComplexPiFrac.getReadableContent() << endl;
        assert(testExactComplexPiFrac.getReadableContent() == "2π/7i");
        //TIVarFile newExactComplexPiFrac = TIVarFile::createNew(TIVarType::createFromName("ExactComplexPiFrac"), "A", TIModel::createFromName("83PCE"));
        //newExactComplexPiFrac.setContentFromString("-42.1337");
        //assert(testExactComplexPiFrac.getRawContent() == newExactComplexPiFrac.getRawContent());
        //newExactComplexPiFrac.saveVarToFile("testData", "Exact_ComplexPiFrac_new");
    }

    {
        TIVarFile testExactComplexRadical = TIVarFile::loadFromFile("testData/Exact_ComplexRadical.8xc");
        cout << "Before: " << testExactComplexRadical.getReadableContent() << endl;

        assert(testExactComplexRadical.getReadableContent() == "(√(6)+√(2))/4+(√(6)-√(2))/4i");
        //TIVarFile newExactComplexRadical = TIVarFile::createNew(TIVarType::createFromName("ExactComplexRadical"), "A", TIModel::createFromName("83PCE"));
        //newExactComplexRadical.setContentFromString("-42.1337");
        //assert(testExactComplexRadical.getRawContent() == newExactComplexRadical.getRawContent());
        //newExactComplexRadical.saveVarToFile("testData", "Exact_ComplexRadical_new");
    }

    {
        TIVarFile testExactRealPi = TIVarFile::loadFromFile("testData/Exact_RealPi.8xn");
        cout << "Before: " << testExactRealPi.getReadableContent() << endl;
        assert(testExactRealPi.getReadableContent() == "30π");
        //TIVarFile newExactRealPi = TIVarFile::createNew(TIVarType::createFromName("ExactRealPi"), "A", TIModel::createFromName("83PCE"));
        //newExactRealPi.setContentFromString("-42.1337");
        //assert(testExactRealPi.getRawContent() == newExactRealPi.getRawContent());
        //newExactRealPi.saveVarToFile("testData", "Exact_RealPi_new");
    }

    {
        TIVarFile testExactRealPiFrac = TIVarFile::loadFromFile("testData/Exact_RealPiFrac.8xn");
        cout << "Before: " << testExactRealPiFrac.getReadableContent() << endl;
        assert(testExactRealPiFrac.getReadableContent() == "2π/7");
        //TIVarFile newExactRealPiFrac = TIVarFile::createNew(TIVarType::createFromName("ExactRealPiFrac"), "A", TIModel::createFromName("83PCE"));
        //newExactRealPiFrac.setContentFromString("-42.1337");
        //assert(testExactRealPiFrac.getRawContent() == newExactRealPiFrac.getRawContent());
        //newExactRealPiFrac.saveVarToFile("testData", "Exact_RealPiFrac_new");
    }

    {
        data_t tmp = data_t({0x0e, 0x00, 0x05, 0x07, 0x43, 0x4f, 0x55, 0x52, 0x41, 0x47, 0x45, 0x43, 0x02, 0x44, 0x6b, 0x32});
        std::string tempEquIfCond = TH_TempEqu::makeStringFromData(tmp);
        assert(tempEquIfCond == "prgmCOURAGE:579:D<2");
    }

    {
        TIVarFile appvarTest = TIVarFile::loadFromFile("testData/AppVar.8xv");
        cout << "appvarTest.getReadableContent() : " << appvarTest.getReadableContent() << endl;
    }

    {
        TIVarFile testPython = TIVarFile::createNew(TIVarType::createFromName("PythonAppVar"), "TEST123", TIModel::createFromName("83PCE"));
        testPython.setContentFromString("from math import *\nprint(math)\n\n# plop");
        testPython.saveVarToFile("testData", "Pythontest_new");

        TIVarFile pythonFromTest = TIVarFile::loadFromFile("testData/Pythontest_new.8xv");
        cout << "pythonFromTest.getReadableContent() : " << pythonFromTest.getReadableContent() << endl;
        assert(pythonFromTest.getReadableContent() == "from math import *\nprint(math)\n\n# plop");

        TIVarFile pythonFromTest2 = TIVarFile::loadFromFile("testData/python_HELLO.8xv");
        cout << "pythonFromTest2.getReadableContent() : " << pythonFromTest2.getReadableContent() << endl;
        assert(pythonFromTest2.getReadableContent() == "import sys\nprint(sys.version)\n");
    }

    return 0;
}
