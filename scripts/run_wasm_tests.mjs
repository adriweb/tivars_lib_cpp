/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const scriptDir = dirname(fileURLToPath(import.meta.url));
const rootDir = resolve(scriptDir, "..");
process.chdir(rootDir);

const { default: TIVarsTests } = await import(pathToFileURL(resolve(rootDir, "tivars_tests_wasm.js")).href);
const module = await TIVarsTests({ noInitialRun: true });

try
{
    module.callMain(process.argv.slice(2));
}
catch (e)
{
    if (e && e.name === "ExitStatus")
    {
        process.exitCode = e.status;
    }
    else if (e && typeof e.excPtr === "number" && module.getExceptionMessage)
    {
        const [type, message] = module.getExceptionMessage(e);
        module.decrementExceptionRefcount(e);
        console.error(`Unhandled C++ exception in wasm tests: ${type}: ${message}`);
        process.exitCode = 1;
    }
    else
    {
        console.error(e);
        process.exitCode = 1;
    }
}
