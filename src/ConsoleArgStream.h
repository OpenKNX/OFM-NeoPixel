#pragma once
/**
 * @file        ConsoleArgStream.h
 * @brief       Minimal whitespace tokenizer for console argument parsing
 *
 * Replaces both argument parsers the console used to rely on, each of which
 * dragged a disproportionate amount of libc/libstdc++ into the image:
 *
 *   std::istringstream  -> instantiates std::basic_ios, which pulls libstdc++'s
 *                          complete locale machinery (num_get/num_put, time_get,
 *                          money_get plus all of their wchar_t twins), ~200 KB
 *   sscanf()            -> pulls newlib's float-capable scanf engine (svfscanf
 *                          and the wide-char/strtoll helpers it needs), ~9 KB
 *
 * Both were used for nothing but splitting a command line at spaces. On a 2 MB
 * RP2350 that was the difference between fitting the firmware region and
 * overflowing it.
 *
 * Semantics match the std::istringstream subset the console relies on:
 *   - whitespace separated tokens
 *   - chained extraction: `if (!(args >> a >> b))`
 *   - once an extraction fails the stream stays failed and every following
 *     extraction is a no-op (sticky failbit)
 *   - integers are read base 10; decimals via readScaled1000() as fixed point
 *
 * Two deliberate divergences from std::istream and sscanf, both in the safe
 * direction:
 *   - the WHOLE token must convert; both read "12abc" as 12 and keep "abc"
 *   - "-1" into an unsigned is an error; both wrap it to the type maximum
 * Such input previously reached the handler as a garbage id and ended in
 * "ERROR: Strip ID not found!"; it now produces the usage hint instead.
 *
 * The sscanf call sites map over as:
 *   sscanf(s, "%d %d", &a, &b) != 2      ->  !(ConsoleArgStream(s) >> a >> b)
 *   int n = sscanf(s, "%d %d %d", ...)   ->  args >> a >> b >> c; n = args.count()
 *   "%*d %*s %u"                         ->  args.skip(2) >> value
 *   "%15s"                               ->  char buf[16]; args >> buf
 *   "%d %d %n" + substr(pos)             ->  args >> a >> b; args.offset()
 *
 * Everything that costs real code lives in ConsoleArgStream.cpp on purpose: the
 * console has ~30 call sites, and inlining the tokenizer into each of them cost
 * more flash than the sscanf engine it replaced.
 *
 * @copyright Copyright (c) 2025 Erkan Colak - OpenKNX (Licensed under GNU GPL v3.0)
 */

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @class ConsoleArgStream
 * @brief Whitespace tokenizer with istream-like extraction operators
 */
class ConsoleArgStream
{
  public:
    explicit ConsoleArgStream(const std::string& args) : _args(args), _pos(0), _count(0), _good(true) {}
    explicit ConsoleArgStream(const char* args) : _args(args ? args : ""), _pos(0), _count(0), _good(true) {}

    /** @brief true while no extraction has failed */
    explicit operator bool() const { return _good; }
    bool operator!() const { return !_good; }

    /** @brief Number of successful extractions so far - the sscanf() return value */
    size_t count() const { return _count; }

    /**
     * @brief Consume tokens without storing them - the sscanf "%*d" / "%*s" suppression
     * @param n Number of tokens to skip; a missing token fails the stream
     */
    ConsoleArgStream& skip(size_t n = 1);

    /**
     * @brief Offset of the unread remainder, whitespace skipped - the sscanf "%n"
     *
     * Points one past the last consumed token with any following whitespace
     * skipped, i.e. at the start of the free-form rest of the line.
     */
    size_t offset() const;

    /** @brief The unread remainder as a string, whitespace skipped */
    std::string rest() const;

    ConsoleArgStream& operator>>(std::string& out);
    /**
     * @brief Read a decimal token scaled by 1000, using integer math only
     * @param out receives value * 1000, so "7.5" yields 7500
     * @return false on a malformed token; more than three decimals are truncated
     * @note Replaces float extraction: strtof drags newlib's string-to-double engine in.
     */
    bool readScaled1000(uint32_t& out);

    // Fundamental types, not the <cstdint> aliases: on this toolchain int32_t is
    // 'long' while 'int' is a distinct type, so aliases alone leave `int`
    // unmatched. Each is a thin narrowing wrapper around one shared extractor -
    // per-type template instantiations would put a full copy of the parser at
    // every call site.
    ConsoleArgStream& operator>>(short& out) { long v; if (extractSigned(v)) out = (short)v; return *this; }
    ConsoleArgStream& operator>>(int& out) { long v; if (extractSigned(v)) out = (int)v; return *this; }
    ConsoleArgStream& operator>>(long& out) { long v; if (extractSigned(v)) out = v; return *this; }
    ConsoleArgStream& operator>>(unsigned char& out) { unsigned long v; if (extractUnsigned(v)) out = (unsigned char)v; return *this; }
    ConsoleArgStream& operator>>(unsigned short& out) { unsigned long v; if (extractUnsigned(v)) out = (unsigned short)v; return *this; }
    ConsoleArgStream& operator>>(unsigned int& out) { unsigned long v; if (extractUnsigned(v)) out = (unsigned int)v; return *this; }
    ConsoleArgStream& operator>>(unsigned long& out) { unsigned long v; if (extractUnsigned(v)) out = v; return *this; }

    /**
     * @brief Read a token into a fixed char buffer - the sscanf "%<N-1>s"
     *
     * Like sscanf the copy is truncated to the buffer size and always NUL
     * terminated. Unlike sscanf the whole token is consumed even when it was too
     * long, so an over-long token cannot spill into the next field.
     */
    template <size_t N>
    ConsoleArgStream& operator>>(char (&buf)[N])
    {
        extractCharBuf(buf, N);
        return *this;
    }

  private:
    /** @brief Read the next whitespace separated token; clears _good at end of input */
    bool nextToken(std::string& token);
    bool extractSigned(long& out);
    bool extractUnsigned(unsigned long& out);
    bool extractCharBuf(char* buf, size_t size);

    std::string _args;
    size_t _pos;
    size_t _count;
    bool _good;
};
