var Module;
// From emscripten: in order to reference the preexisting Module var or create it if needed.
if (!Module) Module = (typeof Module !== 'undefined' ? Module : null) || {};

Module['memoryInitializerPrefixURL'] = '/scripts/z80text/';

Module['onRuntimeInitialized'] = function() {
    Module['ccall']('initlib', 'void', [], []);
};
