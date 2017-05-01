/*
 * Part of tivars_lib_cpp
 * (C) 2015-2017 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include <assert.h>

#include "src/autoloader.h"

#include "src/TIModels.h"
#include "src/TIVarTypes.h"
#include "src/BinaryFile.h"
#include "src/TIVarFile.h"
#include "src/TypeHandlers/TypeHandlers.h"

enum { LANG_EN = 0, LANG_FR };

using namespace std;
using namespace tivars;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /* Init Stuff */

    TIModels::initTIModelsArray();
    TIVarTypes::initTIVarTypesArray();
    TH_0x05::initTokens();


    /* Tests */

    assert(TIVarTypes::getIDFromName("ExactRealPi") == 32);


    TIVarFile testAppVar = TIVarFile::createNew(TIVarType::createFromName("AppVar"), "TEST");
    testAppVar.setContentFromString("ABCD1234C9C8C7C6"); // random but valid hex string
    assert(testAppVar.getReadableContent() == "ABCD1234C9C8C7C6");
    assert(testAppVar.getRawContent().size() == strlen("ABCD1234C9C8C7C6") / 2 + 2);
testAppVar.saveVarToFile("assets/testData", "testAVnew");

    TIVarFile testString = TIVarFile::loadFromFile("assets/testData/String.8xs");
    assert(testString.getReadableContent() == "Hello World");

    TIVarFile testPrgmQuotes = TIVarFile::loadFromFile("assets/testData/testPrgmQuotes.8xp");
    cout << "testPrgmQuotes.getReadableContent() : " << testPrgmQuotes.getReadableContent() << endl;
    assert(testPrgmQuotes.getReadableContent() == "Pause \"2 SECS\",2");

    TIVarFile testEquation = TIVarFile::loadFromFile("assets/testData/Equation_Y1T.8xy");
    assert(testEquation.getReadableContent() == "3sin(T)+4");


    TIVarFile testReal = TIVarFile::loadFromFile("assets/testData/Real.8xn");
    assert(testReal.getReadableContent() == "-42.1337");
    testReal.setContentFromString(".5");
    cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
    assert(testReal.getReadableContent() == "0.5");


    TIVarFile testReal42 = TIVarFile::createNew(TIVarType::createFromName("Real"), "R");
    testReal42.setCalcModel(TIModel::createFromName("84+"));
    testReal42.setContentFromString("9001.42");
    cout << "testReal42.getReadableContent() : " << testReal42.getReadableContent() << endl;
    assert(testReal42.getReadableContent() == "9001.42");
    testReal42.setContentFromString("-0.00000008");
    cout << "testReal42.getReadableContent() : " << testReal42.getReadableContent() << endl;
    assert(atof(testReal42.getReadableContent().c_str()) == -8e-08);
testReal42.saveVarToFile("assets/testData", "Real_new");


    string test = "Disp 42:Wait 5:toString(42):Pause\nInput A,\"?\":Asdf(123)\nFor(I,1,10)\nThen\nDisp I:For(J,1,10)\nThen\nDisp J\nEnd\nEnd";
    cout << "Indented code:" << endl << TH_0x05::reindentCodeString(test) << endl;

#ifndef __EMSCRIPTEN__
    try
    {
        auto goodTypeForCalc = TIVarFile::createNew(TIVarType::createFromName("Program"), "Bla", TIModel::createFromName("83PCE"));
    } catch (runtime_error& e) {
        cout << "Caught unexpected exception: " << e.what() << endl;
    }

    try
    {
        auto badTypeForCalc = TIVarFile::createNew(TIVarType::createFromName("ExactComplexFrac"), "Bla", TIModel::createFromName("84+"));
    } catch (runtime_error& e) {
        cout << "Caught expected exception: " << e.what() << endl;
    }
#endif

    assert(TIVarTypes::getIDFromName("ExactRealPi") == 32);


    TIVarFile testPrgm = TIVarFile::loadFromFile("assets/testData/Program.8xp");
    cout << "testPrgm.getHeader().entries_len = " << testPrgm.getHeader().entries_len << "\t testPrgm.size() - 57 == " << (testPrgm.size() - 57) << endl;
    assert(testPrgm.getHeader().entries_len == testPrgm.size() - 57);
    string testPrgmcontent = testPrgm.getReadableContent({{"lang", LANG_FR}});

    TIVarFile newPrgm = TIVarFile::createNew(TIVarType::createFromName("Program"));
    newPrgm.setContentFromString(testPrgmcontent);
    string newPrgmcontent = newPrgm.getReadableContent({{"lang", LANG_FR}});

    assert(testPrgmcontent == newPrgmcontent);
newPrgm.saveVarToFile("assets/testData", "Program_new");

    cout << endl << "testPrgmcontent : " << endl << testPrgmcontent << endl;


    cout << "\n\n\n" << endl;


    TIVarFile testPrgm42 = TIVarFile::createNew(TIVarType::createFromName("Program"), "asdf");
    testPrgm42.setCalcModel(TIModel::createFromName("82"));
    testPrgm42.setContentFromString("");
    testPrgm42.setVarName("Toto");
testPrgm42.saveVarToFile("assets/testData", "blablaTOTO_new");


    TIVarFile testRealList = TIVarFile::loadFromFile("assets/testData/RealList.8xl");
    cout << "Before: " << testRealList.getReadableContent() << "\n   Now: ";
    testRealList.setContentFromString("{9, 0, .5, -6e-8}");
    cout << testRealList.getReadableContent() << "\n";
testRealList.saveVarToFile("assets/testData", "RealList_new");


    TIVarFile testStandardMatrix = TIVarFile::loadFromFile("assets/testData/Matrix_3x3_standard.8xm");
    cout << "Before: " << testStandardMatrix.getReadableContent() << "\n   Now: ";
    testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7,-8,-9]]");
    testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7.5,-8,-9][1,2,3][4,5,6][-0.002,-8,-9]]");
    cout << testStandardMatrix.getReadableContent() << "\n";
testStandardMatrix.saveVarToFile("assets/testData", "Matrix_new");

    

    TIVarFile testComplex = TIVarFile::loadFromFile("assets/testData/Complex.8xc"); // -5 + 2i
    cout << "Before: " << testComplex.getReadableContent() << "\n   Now: ";
    assert(testComplex.getReadableContent() == "-5+2i");
    TIVarFile newComplex = TIVarFile::createNew(TIVarType::createFromName("Complex"), "C");
    newComplex.setContentFromString("-5+2i");
    assert(newComplex.getRawContent() == newComplex.getRawContent());
    newComplex.setContentFromString("2.5+0.001i");
    cout << "After: " << newComplex.getReadableContent() << endl;
testComplex.saveVarToFile("assets/testData", "Complex_new");



    TIVarFile testComplexList = TIVarFile::loadFromFile("assets/testData/ComplexList.8xl");
    cout << "Before: " << testComplexList.getReadableContent() << "\n   Now: ";
    testComplexList.setContentFromString("{9+2i, 0i, .5, -0.5+6e-8i}");
    cout << testComplexList.getReadableContent() << "\n";
testComplexList.saveVarToFile("assets/testData", "ComplexList_new");



    TIVarFile testExact_RealRadical = TIVarFile::loadFromFile("assets/testData/Exact_RealRadical.8xn");
    cout << "Before: " << testExact_RealRadical.getReadableContent() << endl;
    assert(testExact_RealRadical.getReadableContent() == "(41*√(789)+14*√(654))/259");
    TIVarFile newExact_RealRadical = TIVarFile::createNew(TIVarType::createFromName("ExactRealRadical"), "A", TIModel::createFromName("83PCE"));
//newExact_RealRadical.setContentFromString("-42.1337");
//assert(testExact_RealRadical.getRawContent() == newExact_RealRadical.getRawContent());
//newExact_RealRadical.saveVarToFile("testData", "Exact_RealRadical_new");



    TIVarFile testExactComplexFrac = TIVarFile::loadFromFile("assets/testData/Exact_ComplexFrac.8xc");
    cout << "Before: " << testExactComplexFrac.getReadableContent() << endl;
    assert(testExactComplexFrac.getReadableContent() == "1/5-2/5i");
    TIVarFile newExactComplexFrac = TIVarFile::createNew(TIVarType::createFromName("ExactComplexFrac"), "A", TIModel::createFromName("83PCE"));
//newExactComplexFrac.setContentFromString("-42.1337");
//assert(testExactComplexFrac.getRawContent() == newExactComplexFrac.getRawContent());
//newExactComplexFrac.saveVarToFile("testData", "Exact_ComplexFrac_new");



    TIVarFile testExactComplexPi = TIVarFile::loadFromFile("assets/testData/Exact_ComplexPi.8xc");
    cout << "Before: " << testExactComplexPi.getReadableContent() << endl;
    assert(testExactComplexPi.getReadableContent() == "1/5-3*π*i");
    TIVarFile newExactComplexPi = TIVarFile::createNew(TIVarType::createFromName("ExactComplexPi"), "A", TIModel::createFromName("83PCE"));
//newExactComplexPi.setContentFromString("-42.1337");
//assert(testExactComplexPi.getRawContent() == newExactComplexPi.getRawContent());
//newExactComplexPi.saveVarToFile("testData", "Exact_ComplexPi_new");



    TIVarFile testExactComplexPiFrac = TIVarFile::loadFromFile("assets/testData/Exact_ComplexPiFrac.8xc");
    cout << "Before: " << testExactComplexPiFrac.getReadableContent() << endl;
    assert(testExactComplexPiFrac.getReadableContent() == "2/7*π*i");
    TIVarFile newExactComplexPiFrac = TIVarFile::createNew(TIVarType::createFromName("ExactComplexPiFrac"), "A", TIModel::createFromName("83PCE"));
//newExactComplexPiFrac.setContentFromString("-42.1337");
//assert(testExactComplexPiFrac.getRawContent() == newExactComplexPiFrac.getRawContent());
//newExactComplexPiFrac.saveVarToFile("testData", "Exact_ComplexPiFrac_new");



    TIVarFile testExactComplexRadical = TIVarFile::loadFromFile("assets/testData/Exact_ComplexRadical.8xc");
    cout << "Before: " << testExactComplexRadical.getReadableContent() << endl;
    assert(testExactComplexRadical.getReadableContent() == "((√(6)+√(2))/4)+((√(6)-√(2))/4)*i");
    TIVarFile newExactComplexRadical = TIVarFile::createNew(TIVarType::createFromName("ExactComplexRadical"), "A", TIModel::createFromName("83PCE"));
//newExactComplexRadical.setContentFromString("-42.1337");
//assert(testExactComplexRadical.getRawContent() == newExactComplexRadical.getRawContent());
//newExactComplexRadical.saveVarToFile("testData", "Exact_ComplexRadical_new");



    TIVarFile testExactRealPi = TIVarFile::loadFromFile("assets/testData/Exact_RealPi.8xn");
    cout << "Before: " << testExactRealPi.getReadableContent() << endl;
    assert(testExactRealPi.getReadableContent() == "30*π");
    TIVarFile newExactRealPi = TIVarFile::createNew(TIVarType::createFromName("ExactRealPi"), "A", TIModel::createFromName("83PCE"));
//newExactRealPi.setContentFromString("-42.1337");
//assert(testExactRealPi.getRawContent() == newExactRealPi.getRawContent());
//newExactRealPi.saveVarToFile("testData", "Exact_RealPi_new");



    TIVarFile testExactRealPiFrac = TIVarFile::loadFromFile("assets/testData/Exact_RealPiFrac.8xn");
    cout << "Before: " << testExactRealPiFrac.getReadableContent() << endl;
    assert(testExactRealPiFrac.getReadableContent() == "2/7*π");
    TIVarFile newExactRealPiFrac = TIVarFile::createNew(TIVarType::createFromName("ExactRealPiFrac"), "A", TIModel::createFromName("83PCE"));
//newExactRealPiFrac.setContentFromString("-42.1337");
//assert(testExactRealPiFrac.getRawContent() == newExactRealPiFrac.getRawContent());
//newExactRealPiFrac.saveVarToFile("testData", "Exact_RealPiFrac_new");



    return 0;
}

/*


$testPrgm = TIVarFile::loadFromFile('testData/ProtectedProgram_long.8xp');
$testPrgmcontent = $testPrgm->getReadableContent(['prettify' => true, 'reindent' => true]);
echo "All prettified and reindented:\n" . $testPrgmcontent . "\n";



$testPrgm = TIVarFile::loadFromFile('testData/Program.8xp');
$newPrgm = TIVarFile::createNew(TIVarType::createFromName("Program"));
$newPrgm->setContentFromString($testPrgm->getReadableContent(['lang' => 'en']));
assert($testPrgm->getRawContent() === $newPrgm->getRawContent());




$testExactRealFrac = TIVarFile::loadFromFile('testData/Exact_RealFrac.8xn');
//echo "Before: " . $testExactRealFrac->getReadableContent() . "\t" . "Now: ";
$testExactRealFrac->setContentFromString("0.2");
//echo $testExactRealFrac->getReadableContent() . "\n";
//$testExactRealFrac->saveVarToFile();



//$testMatrixStandard = TIVarFile::loadFromFile('testData/Matrix_3x3_standard.8xm');
//print_r($testMatrixStandard);
//echo "Before: " . $testExactRealFrac->getReadableContent() . "\t" . "Now: ";
//$testExactRealFrac->setContentFromString("0.2");
//echo $testExactRealFrac->getReadableContent() . "\n";
//$testExactRealFrac->saveVarToFile();
*/
