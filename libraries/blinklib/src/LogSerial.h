/*
 * This LogSerial class adds log level capability to the base ServicePortSerial class.  You should be
 * able to replace your existing Serial.h with LogSerial.h and not require any other
 * changes. All base print methods in ServicePortSerial are accessible.
 * The LogSerial class adds 2 new print methods which support a "printf" style format string 
 * and variable arguments.  
 * 
 * There are 5 Log levels (LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG, LOG_ALL).
 
 * E.g.
 * #include "LogSerial.h"
 *  
 * LogSerial lsp;
 * 
 * lsp.print(LOG_ERROR, "Yikes, error on line %d in %s\n", __LINE__, __FILE__);
 * 
 * The first parameter is the LOG_LEVEL, followed by the format string, and a variable number of arguments.
 * The format string may be a const char * (stored in RAM) or a Flash string (PROG_MEM; recommended).
 * The variable arguments can be any argument acceptable to the fprintf method.
 * 
 * Use the setLogLevel method to set the current log level (default Error).  E.g. setLogLevel(LOG_INFO);
 * 
 * Use the enable/disable methods to enable or disable (default) the display of Log message in the Serial Monitor.
 * 
 * To use the LogSerial class:
 * 1) Include "LogSerial.h" (replaces your Serial.h, if needed)
 * 2) Create a LogSerial instance
 * 3) Call the begin method (with baudrate, or empty (default to 500k))
 * 4) Set your log level using setLogLevel method
 * 5) Call enable method to turn on log messaging
 * 6) Use the appropriate print method (either in LogSerial, or the ServicePortSerial class)
 * 
 * CONVENIENCE MACROS:
 * Convenience macros which mirror the Log levels are optionally defined (and customizable).
 * The macros allow you to compile your sketch with or without Log messages (saving program and memory space). 
 * There are 2 sets of macros; (1) uses the format string as-is, (2) wraps the format string
 * in the F() macro so it's stored in Flash memory.
 * 
 * The macros are (first set, use format string as-is):
 * LOG - Always log
 * LOGE - Log an error message
 * LOGW - Log a warning message
 * LOGI - Log an informational message
 * LOGD - Log a debug message
 * 
 * This 2nd set of macros will put the format string in Flash memory:
 * LOGF - Always log
 * LOGFE - Log an error message
 * LOGFW - Log a warning message
 * LOGFI - Log an informational message
 * LOGFD - Log a debug message
 * 
 * The header string (e.g. "Error: ") for the LOGX macros are customizable.  To specify
 * your own header string, override the equivalent #define (below) BEFORE including LogSerial.h. 
 * The default header strings are:
 * 
 * #define LOG_ERROR_HEAD "ERROR: "
 * #define LOG_WARN_HEAD "WARNING: "
 * #define LOG_INFO_HEAD "INFO: "
 * #define LOG_DEBUG_HEAD "DEBUG: "
 *
 * To use the macros, you must tell the pre-processor about your LogSerial instance.  To do this,
 * specify a #define for LOG_SERIAL_INST *before* including the LogSerial.h include file.
 * If LOG_SERIAL_INST is not defined, than these macros become empty statements in your sketch.
 * 
 * To use the LogSerial class and convenience macros (by default, the macros are empty):
 * 
 * 1) #define a LOG_SERIAL_INST *before* the Include.  It should point to your LogSerial instance.
 * 2) Define any "Log header string" overrides
 * 3) Include the LogSerial header file
 * 4) Create a LogSerial instance
 * 5) Call the begin method (with baudrate, or empty (default to 500k))
 * 6) Set your log level using setLogLevel method
 * 7) Call enable method to turn on log messaging
 * 8) Use the appropriate macros or print methods (either in LogSerial, or the ServicePortSerial class)
 * 
 * E.g. Using macros...
 * #define LOG_SERIAL_INST lsp
 * // Specify my own LOG_INFO header string
 * #define LOG_INFO_HEAD "[I]: "
 * 
 * #include "LogSerial.h"
 * 
 * LogSerial lsp;
 * 
 * LOGI("A somewhat informational format string: %s", F("Move along, nothing here"));
 */
#ifndef LogSerial_h

#define LogSerial_h


#include "Serial.h"

enum LogLevel { LOG_ERROR=0, LOG_WARN=1, LOG_INFO=2, LOG_DEBUG=3, LOG_ALL=4 };

/*
 * If you want Log capability, instantiate this class instead of ServicePortSerial.
 */
class LogSerial : public ServicePortSerial {

    public:

    void begin();// defaults to 500k baud rate
    void begin(unsigned long);// specify baud rate (250000 or 500000)

    void setLogLevel(LogLevel);
    LogLevel getLogLevel();
 
    void enable();// enable logging
    void disable();// disable logging

    size_t print(LogLevel, const char *, ...);
    size_t print(LogLevel, const __FlashStringHelper *, ...);

    using Print::print;                     // pull in base print methods

    private:

    bool shouldLog(LogLevel);

    LogLevel logLevel = LogLevel::LOG_ERROR;
    bool enabled = false;
};


#ifdef LOG_SERIAL_INST

// Users can override these header strings
#ifndef LOG_ERROR_HEAD
#define LOG_ERROR_HEAD "ERROR:  "
#endif
#ifndef LOG_WARN_HEAD
#define LOG_WARN_HEAD "WARNING: "
#endif
#ifndef LOG_INFO_HEAD
#define LOG_INFO_HEAD "INFO: "
#endif
#ifndef LOG_DEBUG_HEAD
#define LOG_DEBUG_HEAD "DEBUG: "
#endif

// Use format string as-is
#define LOG(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_ALL, fmt, ##__VA_ARGS__))
#define LOGE(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_ERROR, (LOG_ERROR_HEAD fmt), ##__VA_ARGS__))
#define LOGW(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_WARN, (LOG_WARN_HEAD fmt), ##__VA_ARGS__))
#define LOGI(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_INFO, (LOG_INFO_HEAD Format, ##__VA_ARGS__))
#define LOGD(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_DEBUG, (LOG_DEBUG_HEAD fmt), ##__VA_ARGS__))

// Wrap the format string in F() macro
#define LOGF(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_ALL, F(fmt), ##__VA_ARGS__))
#define LOGFE(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_ERROR, F(LOG_ERROR_HEAD fmt), ##__VA_ARGS__))
#define LOGFW(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_WARN, F(LOG_WARN_HEAD fmt), ##__VA_ARGS__))
#define LOGFI(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_INFO, F(LOG_INFO_HEAD fmt), ##__VA_ARGS__))
#define LOGFD(fmt, ...) (LOG_SERIAL_INST.print(LogLevel::LOG_DEBUG, F(LOG_DEBUG_HEAD fmt), ##__VA_ARGS__))

#else // LOG_SERIAL_INST not defined

#define LOGA(fmt, ...) (void(0))
#define LOGE(fmt, ...) (void(0))
#define LOGW(fmt, ...) (void(0))
#define LOGI(fmt, ...) (void(0))
#define LOGD(fmt, ...) (void(0))

#define LOGFA(fmt, ...) (void(0))
#define LOGFE(fmt, ...) (void(0))
#define LOGFW(fmt, ...) (void(0))
#define LOGFI(fmt, ...) (void(0))
#define LOGFD(fmt, ...) (void(0))

#endif // LOG_SERIAL_INST

#endif