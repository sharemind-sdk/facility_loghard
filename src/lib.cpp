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
#include <map>
#include <memory>
#include <set>
#include <sharemind/compiler-support/GccPR44436.h>
#include <sharemind/compiler-support/GccPR54526.h>
#include <sharemind/facility-module-apis/api_0x1.h>
#include <string>
#include <unordered_map>


namespace {

struct Facility: ::SharemindModuleApi0x1Facility {
    inline Facility(void * const facility, void * const context = nullptr)
        : ::SharemindModuleApi0x1Facility{facility, context}
    {}
    virtual ~Facility() noexcept {}
};
using FacilityPointer = ::std::shared_ptr<Facility>;

struct BackendFacilityBase {
    std::shared_ptr<LogHard::Backend> backend{
        std::make_shared<LogHard::Backend>()};
};

struct BackendFacility: BackendFacilityBase, Facility {
    inline BackendFacility() : Facility{backend.get()} {}
};

struct LoggerFacilityBase { std::shared_ptr<LogHard::Logger> logger; };
struct LoggerFacility: LoggerFacilityBase, Facility {
    template <typename ... T>
    inline LoggerFacility(std::shared_ptr<LogHard::Backend> backend,
                          T && ... prefix)
        : LoggerFacilityBase{
              std::make_shared<LogHard::Logger>(backend,
                                                std::forward<T>(prefix)...)}
        , Facility(logger.get())
    {}
};
struct AppenderFacility: Facility {
    inline AppenderFacility(std::shared_ptr<LogHard::Appender> a)
        : Facility(a.get())
        , appender(std::move(a))
    {}
    std::shared_ptr<LogHard::Appender> const appender;
};

using FacilityMap =
        ::std::unordered_map<SHAREMIND_GCCPR54526_WORKAROUND::std::string,
                             FacilityPointer>;

struct ModuleData {
    inline ModuleData(char const * const conf);
    FacilityMap moduleFacilities;
    FacilityMap pdFacilities;
    FacilityMap pdpiFacilities;
    ::std::set<FacilityPointer> anonBackends;
    ::std::string parsedConfData;
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
    PLACETYPE    ::= module | pd | pdpi
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
void parseConf(ModuleData & data, ::std::string & c) {
    auto const tokens = tokenize(&*c.begin(), &*c.end());
    auto const tend = tokens.cend();
    auto t = tokens.cbegin();

    std::shared_ptr<LogHard::Backend> lastBackend;
    bool backendHasPlace;
    bool backendHasLoggers;
    bool backendHasAppenders;
    bool backendHasPriority;
    bool appenderHasPriority;
    bool loggerHasPlace
            = false; // silence uninitialized warning
    FacilityPointer lastFacility;
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
            lastFacility = std::make_shared<BackendFacility>();
            lastBackend =
                    static_cast<BackendFacility *>(lastFacility.get())->backend;
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
                    if (appenderHasPriority)
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
                    lastBackend->setPriority((level)); \
                    backendHasPriority = true; \
                } else { \
                    assert(lastType == LT_APPENDER); \
                    auto & appenderFacility = \
                        *static_cast<AppenderFacility *>(lastFacility.get()); \
                    appenderFacility.appender->setPriority((level)); \
                    appenderHasPriority = true; \
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
    #define PLACE(where) \
        } else if (ISKEYWORD(#where)) { \
            if ((static_cast<void>(++t), PARSE_END)) \
                throw ParseException{"No " #where " facility name given!"}; \
            auto const r = \
                    data.where ## Facilities.SHAREMIND_GCCPR44436_METHOD( \
                            FacilityMap::value_type{t->str(), lastFacility}); \
            if (!r.second) \
                throw ParseException{"A " #where " facility with this name " \
                                     "already exists!"}; \
            if (lastType == LT_LOGGER) { \
                loggerHasPlace = true; \
            } else if (lastType == LT_BACKEND) { \
                backendHasPlace = true; \
            }
        PLACE(module)
        PLACE(pd)
        PLACE(pdpi)
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
            ;
            {
                auto fileAppender(
                            std::make_shared<LogHard::FileAppender>(t->str(),
                                                                    openMode));
                lastBackend->addAppender(fileAppender);
                lastFacility = std::make_shared<AppenderFacility>(fileAppender);
            }
            backendHasAppenders = true;
            appenderHasPriority = false;
            lastType = LT_APPENDER;
    #define STANDARDAPPENDER(pFILE) \
        } else if (ISKEYWORD("std" #pFILE)) { \
            LOGGERSCHECK; \
            LOGGERPLACECHECK; \
            { \
                auto a(std::make_shared<LogHard::CFileAppender>(std ## pFILE));\
                lastBackend->addAppender(a); \
                lastFacility = std::make_shared<AppenderFacility>(a); \
            } \
            backendHasAppenders = true; \
            appenderHasPriority = false; \
            lastType = LT_APPENDER;
        STANDARDAPPENDER(err)
        STANDARDAPPENDER(out)
        } else if (ISKEYWORD("logger")) {
            if (!backendHasPlace) {
                data.anonBackends.SHAREMIND_GCCPR44436_METHOD(lastFacility);
                backendHasPlace = true;
            }
            LOGGERPLACECHECK;
            if ((static_cast<void>(++t), PARSE_END))
                throw ParseException{"Incomplete \"logger\" definition!"};
            lastFacility =
                    std::make_shared<LoggerFacility>(lastBackend, t->str());
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

inline ModuleData::ModuleData(char const * const conf)
    : parsedConfData{conf ? conf : ""}
{ parseConf(*this, parsedConfData); }

} // anonymous namespace

extern "C" {

SHAREMIND_FACILITY_MODULE_API_MODULE_INFO("LogHardFacility", 1u, 1u);

SHAREMIND_FACILITY_MODULE_API_0x1_INITIALIZER(c,errorStr);
SHAREMIND_FACILITY_MODULE_API_0x1_INITIALIZER(c,errorStr) {
    assert(c);
    try {
        c->moduleHandle = new ModuleData{c->conf};
        return ::SHAREMIND_FACILITY_MODULE_API_0x1_OK;
    } catch (::std::bad_alloc const &) {
        return ::SHAREMIND_FACILITY_MODULE_API_0x1_OUT_OF_MEMORY;
    } catch (ParseException const & e) {
        if (errorStr)
            (*errorStr) = e.what();
        return ::SHAREMIND_FACILITY_MODULE_API_0x1_INVALID_CONFIGURATION;
    } catch (...) {
        return ::SHAREMIND_FACILITY_MODULE_API_0x1_MODULE_ERROR;
    }
}

SHAREMIND_FACILITY_MODULE_API_0x1_DEINITIALIZER(c);
SHAREMIND_FACILITY_MODULE_API_0x1_DEINITIALIZER(c) {
    assert(c);
    assert(c->moduleHandle);
    delete static_cast<ModuleData *>(c->moduleHandle);
}

#define STUFF(name,NAME) \
    SHAREMIND_FACILITY_MODULE_API_0x1_FIND_ ## NAME ## _FACILITY(c, signature);\
    SHAREMIND_FACILITY_MODULE_API_0x1_FIND_ ## NAME ## _FACILITY(c, signature) \
    { \
        assert(c); \
        assert(c->moduleHandle); \
        auto const & map = \
                static_cast<ModuleData *>(c->moduleHandle)->name ## Facilities;\
        auto const it = map.find(signature); \
        return (it == map.cend()) ? nullptr : it->second.get(); \
    }
STUFF(module,MODULE)
STUFF(pd,PD)
STUFF(pdpi,PDPI)

} // extern "C" {
