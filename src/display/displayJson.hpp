#ifndef DISPLAY_JSON_H
#define DISPLAY_JSON_H

#include <string>

// JSON helpers shared by the maps that serve hand-built JSON to their pages.

/**
 * Quote a string as a JSON string literal, escaping what JSON does not allow to appear raw within one.
 * Needed wherever a payload carries text that came from outside the server's own numbers, e.g. a node's name,
 * a chat reply or a script's scene text.
 * @param value String to quote.
 * @returns The quoted and escaped JSON string literal.
 */
std::string jsonString(const std::string& value);

#endif
