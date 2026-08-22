#pragma once

#include_next <FsHelpers.h>

// The firmware header declares each extension check on std::string_view, with
// Arduino String overloads beside some of them. std::string callers are already
// served by the string_view declarations through the standard conversion, so
// this header adds nothing to them.
//
// It previously added a std::string overload per check, which made every call
// passing a plain char* ambiguous -- char* converts to both std::string_view
// and std::string, and neither is better.
