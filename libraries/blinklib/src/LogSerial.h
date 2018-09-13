/*
Supports either 250k or 500k (default) baud rate for the Serial Monitor port. (VSCode max is 250k)

Note: LogSerial does not preclude the use of the ServicePortSerial class or methods.  
However, to realize the above benefits, users may want to transition to LogSerial. 

To use LogSerial functionality:
 1) Define a USE_LOG_LEVEL at the beginning of your sketch.
 2) Include "LogSerial.h"
 3) Call the LOG_SERIAL_BEGIN macro in your setup method (with baudrate, or empty (default to 500k))
 4) Use the appropriate LogSerial macro's

Defining USE_LOG_LEVEL:
To compile away all LogSerial macros, either do not define USE_LOG_LEVEL or set its value to LOG_NONE.
To include all log levels, set USE_LOG_LEVEL to LOG_DEBUG (or LOG_ALL).
To only include errors, set USE_LOG_LEVEL to LOG_ERROR.  (All other log messages are compiled away.)
To only include errors and warnings, set USE_LOG_LEVEL to LOG_WARN. (Info and Debug log messages are compiled away.)
Etc.

LogSerial Macros:
The LogSerial macro composition: 
  // macroName( formatString, variableArgs)
  LOGx ( formatString, ...)

There are 2 sets of macros;
 
Use your format string as-is:
 LOG - Always log
 LOGE - Log an error message
 LOGW - Log a warning message
 LOGI - Log an informational message
 LOGD - Log a debug message
 
Stores your format string in Flash memory:
 LOGF - Always log
 LOGEF - Log an error message
 LOGWF - Log a warning message
 LOGIF - Log an informational message
 LOGDF - Log a debug message
 
Simple use-case:

// Only include error and warning log messages in compilation.
// Information and Debug logging are compiled away.
#define USE_LOG_LEVEL LOG_WARN

#include "LogSerial.h"
 
void setup() {
  // Configure Serial Monitor for 500k baud
  LOG_SERIAL_BEGIN(500000);
}

void loop() {
  // Display error log in Serial Monitor
  LOGEF("Yikes, error on line %d in %s\n", __LINE__, __FILE__);
  // Informational message - compiled away if USE_LOG_LEVEL is below LOG_INFO
  LOGIF("Millis=%lu Value=%x: It works! ", millis(), somevalue); 
} 
 
The header strings for the LOGx macros are customizable by defining your own value.
The default header strings are:
 
 #define LOG_ERROR_HEAD "ERROR: "
 #define LOG_WARN_HEAD "WARNING: "
 #define LOG_INFO_HEAD "INFO: "
 #define LOG_DEBUG_HEAD "DEBUG: "
 */
#ifndef LogSerial_h

#define LogSerial_h

#include "Serial.h"

#define LOG_NONE    0
#define LOG_ERROR   1
#define LOG_WARN    2
#define LOG_INFO    3
#define LOG_DEBUG   4
#define LOG_ALL     LOG_DEBUG

// ensure only chooses what's supported.
enum SerialMonitorBaudRate {
    SM_BAUD_RATE_500K = 500000,
    SM_BAUD_RATE_250K = 250000
};

class LogSerial : public ServicePortSerial {

    public:

    LogSerial(LogSerial const&) = delete;
    void operator=(LogSerial const&) = delete;

   static void init(SerialMonitorBaudRate);// specify baud rate (250000 or 500000)


    static size_t LogSerialPrint(const char *, ...);
    static size_t LogSerialPrint(const __FlashStringHelper *, ...);

    private:

    static LogSerial *getInstance() {
        static LogSerial instance;
        return &instance;
    }

    LogSerial() {}
};

#if (USE_LOG_LEVEL > 0)


#define LOG_SERIAL_PRINT_METHOD LogSerial::LogSerialPrint
#define LOG(fmt, ...) (LOG_SERIAL_PRINT_METHOD(fmt, ##__VA_ARGS__))
#define LOGF(fmt, ...) (LOG_SERIAL_PRINT_METHOD(F(fmt), ##__VA_ARGS__))
#define LOG_SERIAL_BEGIN(enumbaudrate) LogSerial::init(enumbaudrate)

#else // USE_LOG_LEVEL 0

#define LOG(fmt, ...) (void(0))
#define LOGF(fmt, ...) (void(0))
#define LOG_SERIAL_BEGIN(enumbaudrate) (void(0))

#endif // USE_LOG_LEVEL defined at all

#if (USE_LOG_LEVEL >= LOG_ERROR)

    #ifndef LOG_ERROR_HEAD
    // Users can override these header strings
    #define LOG_ERROR_HEAD "ERROR:  "
    #endif

    // #pragma message "Including ERROR logging"
    #define LOGE(fmt, ...) (LOG_SERIAL_PRINT_METHOD((LOG_ERROR_HEAD fmt), ##__VA_ARGS__))
    #define LOGEF(fmt, ...) (LOG_SERIAL_PRINT_METHOD(F(LOG_ERROR_HEAD fmt), ##__VA_ARGS__))

#else // Not including errors

    #define LOGE(fmt, ...) (void(0))
    #define LOGEF(fmt, ...) (void(0))

#endif // Error macros

#if (USE_LOG_LEVEL >= LOG_WARN)

    #ifndef LOG_WARN_HEAD
    // Users can override these header strings
    #define LOG_WARN_HEAD "WARNING: "
    #endif

    // #pragma message "Including WARN logging"
    #define LOGW(fmt, ...) (LOG_SERIAL_PRINT_METHOD((LOG_WARN_HEAD fmt), ##__VA_ARGS__))
    #define LOGWF(fmt, ...) (LOG_SERIAL_PRINT_METHOD(F(LOG_WARN_HEAD fmt), ##__VA_ARGS__))

#else // Not including warnings

    #define LOGW(fmt, ...) (void(0))
    #define LOGWF(fmt, ...) (void(0))
#endif

#if (USE_LOG_LEVEL >= LOG_INFO)

    #ifndef LOG_INFO_HEAD
    // Users can override these header strings
    #define LOG_INFO_HEAD "INFO: "
    #endif

    // #pragma message "Including INFO logging"
    #define LOGI(fmt, ...) (LOG_SERIAL_PRINT_METHOD((LOG_INFO_HEAD fmt), ##__VA_ARGS__))
    #define LOGIF(fmt, ...) (LOG_SERIAL_PRINT_METHOD(F(LOG_INFO_HEAD fmt), ##__VA_ARGS__))

#else // Not including info

    #define LOGI(fmt, ...) (void(0))
    #define LOGIF(fmt, ...) (void(0))

#endif

#if (USE_LOG_LEVEL >= LOG_DEBUG)

    #ifndef LOG_DEBUG_HEAD
    // Users can override these header strings
    #define LOG_DEBUG_HEAD "DEBUG: "
    #endif

    // #pragma message "Including DEBUG logging"
    #define LOGD(fmt, ...) (LOG_SERIAL_PRINT_METHOD((LOG_DEBUG_HEAD fmt), ##__VA_ARGS__))
    #define LOGDF(fmt, ...) (LOG_SERIAL_PRINT_METHOD(F(LOG_DEBUG_HEAD fmt), ##__VA_ARGS__))

#else // not including debug messages

    #define LOGD(fmt, ...) (void(0))
    #define LOGDF(fmt, ...) (void(0))

#endif

#endif // end include file guard
