#include <cassert>
#include <cctype>
#include <cstring>
#include <list>
#include <LogHard/Backend.h>
#include <LogHard/Logger.h>
#include <map>
#include <memory>
#include <sharemind/compiler-support/GccPR54526.h>
#include <sharemind/libfmodapi/api_0x1.h>
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

struct BackendFacility: Facility {
    inline BackendFacility()
        : Facility{new ::LogHard::Backend{}}
        , backend{static_cast< SHAREMIND_GCCPR54526::LogHard::Backend *>(
                      this->facility)}
    {}
    ::std::unique_ptr<SHAREMIND_GCCPR54526::LogHard::Backend> const backend;
};
struct LoggerFacility: Facility {
    template <typename ... T>
    inline LoggerFacility(::LogHard::Backend & backend,
                          T && ... prefix)
        : Facility{new ::LogHard::Logger{backend, ::std::forward<T>(prefix)...}}
        , logger{static_cast<SHAREMIND_GCCPR54526 ::LogHard::Logger *>(
                     this->facility)}
    {}
    ::std::unique_ptr<SHAREMIND_GCCPR54526::LogHard::Logger> logger;
};
struct AppenderFacility: Facility {
    inline AppenderFacility(::LogHard::Backend::Appender * a)
        : Facility{a}
        , appender{a}
    {}
    ::LogHard::Backend::Appender * const appender;
};

using FacilityMap = ::std::unordered_map<SHAREMIND_GCCPR54526::std::string,
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
    if (TOKENIZE_END)
        goto tokenize_end;

    while (::std::isspace(static_cast<unsigned char>(*str))) {
tokenize_skipWhiteSpace:
        if ((++str, TOKENIZE_END))
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
            if (TOKENIZE_END) { \
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
        if (TOKENIZE_END)
            goto tokenize_end;
        if (::std::isspace(static_cast<unsigned char>(*str)))
            goto tokenize_skipWhiteSpace;
        throw ParseException{"Garbage after quoted string!"};
    } else {
        // Handle normal strings:
        const char * const start = str;
        do {
            if ((++str, TOKENIZE_END)) {
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
    PLACETYPE    ::= module | pd | pdpi
    PLACE        ::= PLACETYPE <facilityName>
    LOGGER       ::= logger <logPrefix> PLACE+
    FILEOPENMODE ::= append | overwrite
    FILEAPPENDER ::= file FILEOPENMODE <fileName> PLACE*
    APPENDER     ::= FILEAPPENDER | stderr | stdout
    BACKEND      ::= backend PLACE* LOGGER+ APPENDER+ (LOGGER | APPENDER)*
    CONFIG       ::= BACKEND*
*/

#define PARSE_END (t == tend)
#define ISKEYWORD(k) (!t->quoted() && ::std::strcmp(t->str(), (k)) == 0)
void parseConf(ModuleData & data, ::std::string & c) {
    auto const tokens = tokenize(&*c.begin(), &*c.end());
    auto const tend = tokens.cend();
    auto t = tokens.cbegin();

    ::LogHard::Backend * lastBackend;
    bool backendHasPlace;
    bool backendHasLoggers;
    bool backendHasAppenders;
    bool loggerHasPlace
            = false; // silence uninitialized warning
    FacilityPointer lastFacility;
    enum { LT_NONE, LT_BACKEND, LT_APPENDER, LT_LOGGER } lastType = LT_NONE;

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

    while ((++t, !PARSE_END)) {
        assert(lastType != LT_NONE);
        if (ISKEYWORD("backend")) {
            if (!backendHasAppenders)
                throw ParseException{"A \"backend\" has defined no appenders!"};
    parseConf_handleBackend:
            backendHasPlace = false;
            backendHasLoggers = false;
            backendHasAppenders = false;
            BackendFacility * const bf = new BackendFacility{};
            lastFacility.reset(bf);
            lastBackend = bf->backend.get();
            lastType = LT_BACKEND;
    #define PLACE(where) \
        } else if (ISKEYWORD(#where)) { \
            if ((++t, PARSE_END)) \
                throw ParseException{"No " #where " facility name given!"}; \
            auto const r = \
                    data.where ## Facilities.emplace( \
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
            if ((++t, PARSE_END))
                throw ParseException{"Incomplete \"file\" definition!"};
            using FA = ::LogHard::Backend::FileAppender;
            FA::OpenMode const openMode = [t] {
                if (ISKEYWORD("append"))
                    return FA::APPEND;
                if (ISKEYWORD("overwrite"))
                    return FA::OVERWRITE;
                throw ParseException{"Invalid \"file\" open mode given!"};
            }();
            if ((++t, PARSE_END))
                throw ParseException{"Incomplete \"file\" definition!"};
            FA * const fa = new FA{t->str(), openMode};
            try {
                lastFacility.reset(new AppenderFacility{fa});
            } catch (...) {
                delete fa;
                throw;
            }
            lastBackend->addAppender(fa);
            backendHasAppenders = true;
            lastType = LT_APPENDER;
    #define STANDARDAPPENDER(pFILE) \
        } else if (ISKEYWORD(#pFILE)) { \
            LOGGERSCHECK; \
            LOGGERPLACECHECK; \
            using CFA = ::LogHard::Backend::CFileAppender; \
            CFA * const cfa = new CFA{pFILE}; \
            try { \
                lastFacility.reset(new AppenderFacility{cfa}); \
            } catch (...) { \
                delete cfa; \
                throw; \
            } \
            lastBackend->addAppender(cfa); \
            backendHasAppenders = true; \
            lastType = LT_APPENDER;
        STANDARDAPPENDER(stderr)
        STANDARDAPPENDER(stdout)
        } else if (ISKEYWORD("logger")) {
            if (!backendHasPlace) {
                data.anonBackends.emplace(lastFacility);
                backendHasPlace = true;
            }
            LOGGERPLACECHECK;
            if ((++t, PARSE_END))
                throw ParseException{"Incomplete \"logger\" definition!"};
            lastFacility.reset(new LoggerFacility{*lastBackend, t->str()});
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
