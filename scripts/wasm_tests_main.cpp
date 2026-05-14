/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include <exception>
#include <iostream>

int tivars_tests_main(int argc, char** argv);

int main(int argc, char** argv)
{
    try
    {
        return tivars_tests_main(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unhandled C++ exception in wasm tests: " << e.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unhandled non-standard exception in wasm tests\n";
        return 1;
    }
}
