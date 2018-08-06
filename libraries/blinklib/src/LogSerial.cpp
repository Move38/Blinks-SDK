/*
 * This LogSerial class adds log level capability to the base ServicePortSerial class.  You should be
 * able to replace your existing Serial.h with LogSerial.h and not require any other
 * changes. All base print methods in ServicePortSerial are accessible.
 * The LogSerial class adds 2 new print methods which support a "printf" style format string 
 * and variable arguments.  
 */

#include <stdio.h>
#include "LogSerial.h"

static FILE _logOutput = {0};
static LogSerial *_logSerial;

static int logSerial_Putc(char c, FILE *file) {
    _logSerial->write(c);
}

void LogSerial::begin() {
    begin(500000);
}

void LogSerial::begin(unsigned long _baudrate) {

    _logSerial=this;
    ServicePortSerial::begin(_baudrate);
    fdev_setup_stream(&_logOutput, logSerial_Putc, NULL, _FDEV_SETUP_WRITE);
    stdout = stderr = &_logOutput;
}

void LogSerial::setLogLevel(LogLevel level) {
    logLevel = level;
}

void LogSerial::enable() { enabled = true; }

void LogSerial::disable() { enabled = false; }

LogLevel LogSerial::getLogLevel() { 
    return logLevel; 
}

bool LogSerial::shouldLog(LogLevel level) {
    if(enabled && (logLevel >= level || level == LOG_ALL))
        return true;

    return false;
}

size_t LogSerial::print (LogLevel level, const char *fmt, ...) {
    va_list vp;
    int i;

    if(!shouldLog(level))
        return 0;
    va_start(vp, fmt);
	i = vfprintf(stdout, fmt, vp);
	va_end(vp);

	return i;
}

size_t LogSerial::print (LogLevel level, const __FlashStringHelper *fmt, ...) {
    va_list vp;
    int i;

    if(!shouldLog(level))
        return 0;
    va_start(vp, fmt);
 	stdout->flags |= __SPGM;// using prog_mem
	i = vfprintf_P(stdout, reinterpret_cast<const char *>(fmt), vp);
	stdout->flags &= ~__SPGM;
	va_end(vp);

	return i;
}
