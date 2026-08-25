/**
 * @file        ConsoleArgStream.cpp
 * @brief       Implementation of the console argument tokenizer
 *
 * Deliberately out-of-line: the console has around 30 extraction call sites and
 * an inline tokenizer costs more flash than the sscanf engine it replaced.
 * See ConsoleArgStream.h for the semantics.
 *
 * @copyright Copyright (c) 2025 Erkan Colak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#include "ConsoleArgStream.h"

#include <cstdlib>

namespace
{
    inline bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
} // namespace

bool ConsoleArgStream::nextToken(std::string& token)
{
    if (!_good) return false;
    while (_pos < _args.size() && isSpace(_args[_pos]))
        _pos++;
    if (_pos >= _args.size())
    {
        _good = false;
        return false;
    }
    const size_t start = _pos;
    while (_pos < _args.size() && !isSpace(_args[_pos]))
        _pos++;
    token.assign(_args, start, _pos - start);
    return true;
}

ConsoleArgStream& ConsoleArgStream::skip(size_t n)
{
    std::string token;
    while (n-- && nextToken(token))
        ;
    return *this;
}

size_t ConsoleArgStream::offset() const
{
    size_t p = _pos;
    while (p < _args.size() && isSpace(_args[p]))
        p++;
    return p;
}

std::string ConsoleArgStream::rest() const
{
    return _args.substr(offset());
}

ConsoleArgStream& ConsoleArgStream::operator>>(std::string& out)
{
    std::string token;
    if (!nextToken(token)) return *this;
    out = token;
    _count++;
    return *this;
}

bool ConsoleArgStream::readScaled1000(uint32_t& out)
{
    std::string token;
    if (!nextToken(token)) return false;

    uint32_t whole = 0;
    uint32_t frac = 0;
    uint32_t fracDigits = 0;
    bool seenDigit = false;
    bool inFrac = false;

    for (char c : token)
    {
        if (c == '.' || c == ',')
        {
            if (inFrac) { _good = false; return false; }
            inFrac = true;
            continue;
        }
        if (c < '0' || c > '9') { _good = false; return false; }
        seenDigit = true;
        if (inFrac)
        {
            if (fracDigits < 3) { frac = frac * 10 + (uint32_t)(c - '0'); fracDigits++; }
        }
        else
        {
            if (whole > 4000000) { _good = false; return false; } // keep whole * 1000 in range
            whole = whole * 10 + (uint32_t)(c - '0');
        }
    }
    if (!seenDigit) { _good = false; return false; }

    while (fracDigits < 3) { frac *= 10; fracDigits++; }

    out = whole * 1000 + frac;
    _count++;
    return true;
}

bool ConsoleArgStream::extractSigned(long& out)
{
    std::string token;
    if (!nextToken(token)) return false;
    char* end = nullptr;
    const long value = strtol(token.c_str(), &end, 10);
    if (end == token.c_str() || *end != '\0')
    {
        _good = false;
        return false;
    }
    out = value;
    _count++;
    return true;
}

bool ConsoleArgStream::extractUnsigned(unsigned long& out)
{
    std::string token;
    if (!nextToken(token)) return false;
    // reject "-1" explicitly: strtoul would silently wrap it to the type maximum
    if (token[0] == '-')
    {
        _good = false;
        return false;
    }
    char* end = nullptr;
    const unsigned long value = strtoul(token.c_str(), &end, 10);
    if (end == token.c_str() || *end != '\0')
    {
        _good = false;
        return false;
    }
    out = value;
    _count++;
    return true;
}

bool ConsoleArgStream::extractCharBuf(char* buf, size_t size)
{
    std::string token;
    if (!nextToken(token)) return false;
    const size_t len = token.size() < size - 1 ? token.size() : size - 1;
    token.copy(buf, len);
    buf[len] = '\0';
    _count++;
    return true;
}
