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
#include "src/TypeHandlers/TH_0x05.h"
#include "src/TypeHandlers/TH_0x00.h"

using namespace std;
using namespace tivars;

int main(int argc, char** argv)
{
    /* Init Stuff */

    TIModels::initTIModelsArray();
    TIVarTypes::initTIVarTypesArray();
    TH_0x05::initTokens();


    /* Tests */

    /*
    BinaryFile bf("/Users/adriweb/Documents/tivars_lib_cpp/testData/Complex.8xc");
    data_t someData = bf.get_raw_bytes(11);
    string teststrbytes = bf.get_string_bytes(42);


    assert(TIVarTypes::getIDFromName("ExactRealPi") == 32);


    string test = "Disp 42:Pause\nInput A,\"?\":Asdf(123)\nFor(I,1,10)\nThen\nDisp I:For(J,1,10)\nThen\nDisp J\nEnd\nEnd";
    cout << "Indented code:" << endl << TH_0x05::reindentCodeString(test) << endl;


    string strFromData = TH_0x05::makeStringFromData({7, 0, 187, 106, 95, 65, 66, 67, 68}, {});
    cout << "string from data: " << endl << strFromData << endl;
    cout << endl;


    cout << "data from string: " << endl;
    data_t testData = TH_0x05::makeDataFromString("Asm(prgmABCD", {});
    for (int i = 0; i < testData.size(); ++i)
    {
        cout << (uint)testData[i] << endl;
    }
    cout << endl;


    cout << "Raw data for Real == 45.2:" << endl;
    data_t testData2 = TH_0x00::makeDataFromString("45.2", {});
    for (int i = 0; i < testData2.size(); ++i)
    {
        cout << (uint)testData2[i] << endl;
    }


    try
    {
        auto goodTypeForCalc = TIVarFile::createNew(TIVarType::createFromName("Program"), "Bla", TIModel::createFromName("83PCE"));
        goodTypeForCalc.setContentFromData(testData);
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
    */

    TIVarFile testPrgm = TIVarFile::loadFromFile("/Users/adriweb/Documents/tivars_lib_cpp/testData/Program.8xp");
    cout << "testPrgm.getHeader().entries_len = " << testPrgm.getHeader().entries_len << "\t testPrgm.size() - 57 == " << (testPrgm.size() - 57) << endl;
    assert(testPrgm.getHeader().entries_len == testPrgm.size() - 57);
    TIVarFile newPrgm = TIVarFile::createNew(TIVarType::createFromName("Program"));
    string testPrgmcontent = testPrgm.getReadableContent({{"lang", LANG_FR}});
    cout << "testPrgmContent :" << endl << testPrgmcontent << endl;
    newPrgm.setContentFromString(testPrgmcontent, {});
    string newPrgmcontent = newPrgm.getReadableContent({{"lang", LANG_FR}});
    cout << "newPrgmcontent :" << endl << newPrgmcontent << endl;
    cout << "testPrgm.getInstanceChecksum() = " << testPrgm.getInstanceChecksum() << endl;
    cout << "newPrgm.getInstanceChecksum() = " << newPrgm.getInstanceChecksum() << endl;
    assert(testPrgm.getInstanceChecksum() == newPrgm.getInstanceChecksum());
    //$newPrgm->saveVarToFile();


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



$testReal = TIVarFile::loadFromFile('testData/Real.8xn'); // -42.1337
$newReal = TIVarFile::createNew(TIVarType::createFromName("Real"), "A");
$newReal->setContentFromString('-42.1337');
assert($testReal->getReadableContent() === '-42.1337');
assert($testReal->getRawContent() === $newReal->getRawContent());
//$newReal->saveVarToFile("/Users/adriweb/", "trololol");



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
