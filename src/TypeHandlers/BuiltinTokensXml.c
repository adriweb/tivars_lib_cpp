/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include <stddef.h>

const unsigned char tivars_builtin_tokens_xml[] = {
#if defined(__has_embed)
#  if __has_embed("../../ti-toolkit-8x-tokens.xml")
#    embed "../../ti-toolkit-8x-tokens.xml"
#  else
#    error "ti-toolkit-8x-tokens.xml not found"
#  endif
#else
#  error "This compiler does not support #embed"
#endif
};

const size_t tivars_builtin_tokens_xml_size = sizeof(tivars_builtin_tokens_xml);
