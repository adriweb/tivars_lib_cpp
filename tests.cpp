/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
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
    /* Init Stuff */

    TIModels::initTIModelsArray();
    TIVarTypes::initTIVarTypesArray();
    TH_0x05::initTokens();


    /* Tests */

    assert(TIVarTypes::getIDFromName("ExactRealPi") == 32);


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
    //testReal42.saveVarToFile("/Users/adriweb/Downloads", "Real9001dot42");


    string test = "Disp 42:Wait 5:toString(42):Pause\nInput A,\"?\":Asdf(123)\nFor(I,1,10)\nThen\nDisp I:For(J,1,10)\nThen\nDisp J\nEnd\nEnd";
    cout << "Indented code:" << endl << TH_0x05::reindentCodeString(test) << endl;


    try
    {
        auto goodTypeForCalc = TIVarFile::createNew(TIVarType::createFromName("Program"), "Bla", TIModel::createFromName("83PCE"));
    } catch (runtime_error e) {
        cout << "Caught unexpected exception: " << e.what() << endl;
    }

    try
    {
        auto badTypeForCalc = TIVarFile::createNew(TIVarType::createFromName("ExactComplexFrac"), "Bla", TIModel::createFromName("84+"));
    } catch (runtime_error e) {
        cout << "Caught expected exception: " << e.what() << endl;
    }


    assert(TIVarTypes::getIDFromName("ExactRealPi") == 32);


    TIVarFile testPrgm = TIVarFile::loadFromFile("assets/testData/Program.8xp");
    cout << "testPrgm.getHeader().entries_len = " << testPrgm.getHeader().entries_len << "\t testPrgm.size() - 57 == " << (testPrgm.size() - 57) << endl;
    assert(testPrgm.getHeader().entries_len == testPrgm.size() - 57);
    string testPrgmcontent = testPrgm.getReadableContent({{"lang", LANG_FR}});

    TIVarFile newPrgm = TIVarFile::createNew(TIVarType::createFromName("Program"));
    newPrgm.setContentFromString(testPrgmcontent);
    string newPrgmcontent = newPrgm.getReadableContent({{"lang", LANG_FR}});

    assert(testPrgmcontent == newPrgmcontent);
    //newPrgm.saveVarToFile();

    cout << endl << "testPrgmcontent : " << endl << testPrgmcontent << endl;


    cout << "\n\n\n" << endl;


    TIVarFile testPrgm42 = TIVarFile::createNew(TIVarType::createFromName("Program"), "asdf");
    testPrgm42.setCalcModel(TIModel::createFromName("82"));
    testPrgm42.setContentFromString("");
    testPrgm42.setVarName("Toto");
    //testPrgm42.saveVarToFile("/Users/adriweb/Downloads", "blablaTOTO");


    TIVarFile testRealList = TIVarFile::loadFromFile("assets/testData/RealList.8xl");
    cout << "Before: " << testRealList.getReadableContent() << "\t" << "Now: ";
    testRealList.setContentFromString("{9, 0, .5, -6e-8}");
    cout << testRealList.getReadableContent() << "\n";
//testRealList.saveVarToFile("testData", "RealList_new");


    TIVarFile testStandardMatrix = TIVarFile::loadFromFile("assets/testData/Matrix_3x3_standard.8xm");
    cout << "Before: " << testStandardMatrix.getReadableContent() << "\t" << "Now: ";
    testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7,-8,-9]]");
    testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7,-8,-9][1,2,3][4,5,6][-7,-8,-9]]");
    cout << testStandardMatrix.getReadableContent() << "\n";
//testStandardMatrix.saveVarToFile('testData', 'Matrix_new');


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
