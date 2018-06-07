/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#include <cassert>
#include <cctype>
#include <cstring>
#include <list>
#include <LogHard/Backend.h>
#include <LogHard/CFileAppender.h>
#include <LogHard/FileAppender.h>
#include <LogHard/Logger.h>
#include <memory>
#include <sharemind/facility-module-apis/api.h>
#include <sharemind/facility-module-apis/api_0x2.h>
#include <sharemind/SimpleUnorderedStringMap.h>
#include <string>
#include <type_traits>
#include <utility>


namespace {

namespace V2 = sharemind::FacilityModuleApis::v2;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

struct FacilityBase {

    virtual std::shared_ptr<void> getFacilityPtr(std::shared_ptr<FacilityBase>)
            noexcept = 0;

};

#pragma GCC diagnostic pop

template <typename T>
struct Facility final : FacilityBase, T {

    template <typename ... Args>
    Facility(Args && ... args)
            noexcept(std::is_nothrow_constructible<T, Args...>::value)
        : T(std::forward<Args>(args)...)
    {}

    std::shared_ptr<void> getFacilityPtr(std::shared_ptr<FacilityBase> self)
                noexcept final override
    {
        assert(self.get() == this);
        return std::shared_ptr<void>(std::move(self), static_cast<T *>(this));
    }

};

class Token {

public: /* Methods: */

    inline Token(char const * const begin,
                 bool const isQuoted)
        : m_str{begin}
        , m_isQuoted{isQuoted}
    {}

    char const * const & str() const noexcept { return m_str; }
    bool quoted() const noexcept { return m_isQuoted; }

private: /* Fields: */

    char const * const m_str;
    bool const m_isQuoted;

};

class ParseException : public ::std::exception {

public: /* Methods: */

    inline ParseException(char const * const msg) noexcept : m_msg(msg) {}
    inline char const * what() const noexcept override { return m_msg; }

private: /* Fields: */

    char const * const m_msg;

};

#define TOKENIZE_END (str == end)
::std::list<Token> tokenize(char * const begin, char * const end) {
    ::std::list<Token> tokens;
    char * str{begin};
    if TOKENIZE_END
        goto tokenize_end;

    while (::std::isspace(static_cast<unsigned char>(*str))) {
tokenize_skipWhiteSpace:
        ++str;
        if TOKENIZE_END
            goto tokenize_end;
    }

    // Handle tokens:
    assert(!::std::isspace(static_cast<unsigned char>(*str)));

    if ((*str == '"') || (*str == '\'')) {
        // Handle quoted strings:
        char const quote = *str;
        char * writePtr = ++str;
        const char * const start = writePtr;

        #define FAILONEND \
            if TOKENIZE_END { \
                throw ParseException{"Unterminated quoted string!"}; \
            } else (void) false
        for (;;) {
            FAILONEND;
            if (*str == quote) {
                *writePtr = '\0';
                tokens.emplace_back(start, true);
                ++str;
                break;
            } else if (*str == '\\') {
                ++str;
                FAILONEND;
                #define writeEscape(c,e) case c: *writePtr = e; break
                switch (*str) {
                    writeEscape('a', '\a'); writeEscape('b', '\b');
                    writeEscape('f', '\f'); writeEscape('n', '\n');
                    writeEscape('r', '\r'); writeEscape('t', '\t');
                    writeEscape('v', '\v'); writeEscape('\\', '\\');
                    writeEscape('?', '\?'); writeEscape('\"', '\"');
                    writeEscape('\'', '\'');
                    case 'x':
                    {
                        ++str;
                        FAILONEND;
                        char base;
                        #define GETHEXBASE(base) \
                            switch (*str) { \
                                case '0' ... '9': base = '0'; break; \
                                case 'a' ... 'f': base = 'a' - 10; break; \
                                case 'A' ... 'F': base = 'A' - 10; break; \
                                default: \
                                    throw ParseException{ \
                                            "Invalid \\x escape!"}; \
                            }
                        GETHEXBASE(base)
                        unsigned const v =
                                static_cast<unsigned>(*str - base) * 16u;
                        ++str;
                        FAILONEND;
                        GETHEXBASE(base)
                        (*writePtr) =
                                static_cast<char>(static_cast<unsigned char>(
                                    v + static_cast<unsigned>(*str - base)));
                        break;
                    }
                    default:
                        *writePtr = *str;
                        break;
                }
            } else if (writePtr != str) {
                *writePtr = *str;
            }
            ++writePtr;
            ++str;
        }
        if TOKENIZE_END
            goto tokenize_end;
        if (::std::isspace(static_cast<unsigned char>(*str)))
            goto tokenize_skipWhiteSpace;
        throw ParseException{"Garbage after quoted string!"};
    } else {
        // Handle normal strings:
        const char * const start = str;
        do {
            ++str;
            if TOKENIZE_END {
                tokens.emplace_back(start, false);
                goto tokenize_end;
            }
        } while (!::std::isspace(static_cast<unsigned char>(*str)));
        *str = '\0';
        tokens.emplace_back(start, false);
        goto tokenize_skipWhiteSpace;
    }

tokenize_end:
    return tokens;
}

/*
    PRIO_LEVEL   ::= fatal | error | warning | normal | debug | fulldebug
    PRIORITY     ::= priority <PRIO_LEVEL>
    PLACETYPE    ::= facilitymodule | module | pd | pdpi | process
    PLACE        ::= PLACETYPE <facilityName>
    LOGGER       ::= logger <logPrefix> PLACE+
    FILEOPENMODE ::= append | overwrite
    FILEAPPENDER ::= file FILEOPENMODE <fileName>
    APPENDER     ::= (FILEAPPENDER | stderr | stdout) PLACE* PRIORITY? PLACE*
    BACKEND      ::=
        backend PLACE* PRIORITY? PLACE* LOGGER+ APPENDER+ (LOGGER | APPENDER)*
    CONFIG       ::= BACKEND*
*/

#define PARSE_END (t == tend)
#define ISKEYWORD(k) (!t->quoted() && ::std::strcmp(t->str(), (k)) == 0)
void parseConf(V2::ModuleInitContext & context) {
    auto conf(context.moduleConfigurationString());
    auto const tokens = tokenize(&*conf.begin(), &*conf.end());
    auto const tend = tokens.cend();
    auto t = tokens.cbegin();

    std::shared_ptr<LogHard::Backend> lastBackend;
    std::shared_ptr<LogHard::Appender> lastAppenderWithoutPriority;
    bool backendHasPlace;
    bool backendHasLoggers;
    bool backendHasAppenders;
    bool backendHasPriority;
    bool loggerHasPlace
            = false; // silence uninitialized warning
    std::shared_ptr<FacilityBase> lastFacility;
    enum { LT_BACKEND, LT_APPENDER, LT_LOGGER } lastType;

    if (PARSE_END)
        return;
    if (!ISKEYWORD("backend"))
        throw ParseException{"Configuration must start with \"backend\"!"};


#define LOGGERPLACECHECK \
        if (lastType == LT_LOGGER && !loggerHasPlace) { \
            throw ParseException{"A \"logger\" was not registered as a " \
                                 "facility!"}; \
        } else (void) 0
#define LOGGERSCHECK \
    if (!backendHasLoggers) { \
        throw ParseException{"At least one \"logger\" must be defined for a " \
                             "backend\" before appenders can be defined!"}; \
    } else (void) 0

    goto parseConf_handleBackend;

    while ((static_cast<void>(++t), !PARSE_END)) {
        if (ISKEYWORD("backend")) {
            LOGGERPLACECHECK;
            if (!backendHasAppenders)
                throw ParseException{"A \"backend\" has defined no appenders!"};
    parseConf_handleBackend:
            backendHasPlace = false;
            backendHasLoggers = false;
            backendHasAppenders = false;
            backendHasPriority = false;
            {
                auto backend(std::make_shared<Facility<LogHard::Backend> >());
                lastBackend = backend;
                lastFacility = std::move(backend);
            }
            lastType = LT_BACKEND;
        } else if (ISKEYWORD("priority")) {
            switch (lastType) {
                case LT_BACKEND:
                    if (backendHasPriority)
                        throw ParseException(
                                "Log priority already given for this backend!");
                    break;
                case LT_LOGGER:
                    throw ParseException(
                            "Loggers don't yet support log priorities!");
                case LT_APPENDER:
                    if (!lastAppenderWithoutPriority)
                        throw ParseException("Log priority already given for "
                                             "this appender!");
                    break;
            }
            if ((static_cast<void>(++t), PARSE_END))
                throw ParseException("No priority level name given!");
            if (t->quoted())
                throw ParseException("Expected priority level keyword, "
                                     "but string was given!");
    #define SETLOGPRIORITY(keyword, level) \
            if (std::strcmp(t->str(), (keyword))) { \
                if (lastType == LT_BACKEND) { \
                    assert(lastBackend); \
                    lastBackend->setPriority((level)); \
                    backendHasPriority = true; \
                } else { \
                    assert(lastType == LT_APPENDER); \
                    assert(lastAppenderWithoutPriority); \
                    lastAppenderWithoutPriority->setPriority((level)); \
                    lastAppenderWithoutPriority.reset(); \
                } \
            }
            SETLOGPRIORITY("fatal", LogHard::Priority::Fatal)
            else SETLOGPRIORITY("error", LogHard::Priority::Error)
            else SETLOGPRIORITY("warning", LogHard::Priority::Warning)
            else SETLOGPRIORITY("normal", LogHard::Priority::Normal)
            else SETLOGPRIORITY("debug", LogHard::Priority::Debug)
            else SETLOGPRIORITY("fulldebug", LogHard::Priority::FullDebug)
    #undef SETLOGPRIORITY
            else throw ParseException("Invalid log priority given!");
    #define PLACE(kw,Where,e) \
        } else if (ISKEYWORD(kw)) { \
            if ((static_cast<void>(++t), PARSE_END)) \
                throw ParseException{"No " e " facility name given!"}; \
            context.register ## Where ## Facility( \
                    t->str(), \
                    lastFacility->getFacilityPtr(lastFacility)); \
            if (lastType == LT_LOGGER) { \
                loggerHasPlace = true; \
            } else if (lastType == LT_BACKEND) { \
                backendHasPlace = true; \
            }
        PLACE("facilitymodule", FacilityModule, "facility module")
        PLACE("module", Module, "module")
        PLACE("pd", Pd, "PD")
        PLACE("pdpi", Pdpi, "PDPI")
        PLACE("process", Process, "process")
    #undef PLACE
        } else if (ISKEYWORD("file")) {
            LOGGERSCHECK;
            LOGGERPLACECHECK;
            if ((static_cast<void>(++t), PARSE_END))
                throw ParseException{"Incomplete \"file\" definition!"};
            LogHard::FileAppender::OpenMode const openMode([t] {
                if (ISKEYWORD("append"))
                    return LogHard::FileAppender::APPEND;
                if (ISKEYWORD("overwrite"))
                    return LogHard::FileAppender::OVERWRITE;
                throw ParseException{"Invalid \"file\" open mode given!"};
            }());
            if ((static_cast<void>(++t), PARSE_END))
                throw ParseException{"Incomplete \"file\" definition!"};
            {
                auto fileAppender(
                            std::make_shared<Facility<LogHard::FileAppender> >(
                                t->str(),
                                openMode));
                lastBackend->addAppender(fileAppender);
                lastAppenderWithoutPriority = fileAppender;
                lastFacility = std::move(fileAppender);
            }
            backendHasAppenders = true;
            lastType = LT_APPENDER;
    #define STANDARDAPPENDER(pFILE) \
        } else if (ISKEYWORD("std" #pFILE)) { \
            LOGGERSCHECK; \
            LOGGERPLACECHECK; \
            { \
                auto appender( \
                        std::make_shared<Facility<LogHard::CFileAppender> >( \
                                std ## pFILE));\
                lastBackend->addAppender(appender); \
                lastAppenderWithoutPriority = appender; \
                lastFacility = std::move(appender); \
            } \
            backendHasAppenders = true; \
            lastType = LT_APPENDER;
        STANDARDAPPENDER(err)
        STANDARDAPPENDER(out)
    #undef STANDARDAPPENDER
        } else if (ISKEYWORD("logger")) {
            if (!backendHasPlace)
                backendHasPlace = true;
            LOGGERPLACECHECK;
            if ((static_cast<void>(++t), PARSE_END))
                throw ParseException{"Incomplete \"logger\" definition!"};
            lastFacility =
                    std::make_shared<Facility<LogHard::Logger> >(lastBackend,
                                                                 t->str());
            lastType = LT_LOGGER;
            backendHasLoggers = true;
            loggerHasPlace = false;
        } else {
            throw ParseException{"Unknown keyword!"};
        }
    }
    if (!backendHasAppenders)
        throw ParseException{"A \"backend\" has defined no appenders!"};
}

} // anonymous namespace

extern "C" {

SHAREMIND_FACILITY_MODULE_API_MODULE_INFO("LogHardFacility", 2u, 2u);
extern V2::FacilityModuleInfo sharemindFacilityModuleInfo_v2;
V2::FacilityModuleInfo sharemindFacilityModuleInfo_v2{parseConf};

} // extern "C" {
