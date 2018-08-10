
/*
 * LogSerial.cpp
 */

// Just has to be non-zero to pull in the class
#define USE_LOG_LEVEL LOG_ERROR

#include <stdio.h>
#include "LogSerial.h"

static LogSerial *_logSerial = NULL;

static int logSerial_Putc(char c, FILE *file) {
    _logSerial->write(c);
}

void LogSerial::init() {
    init(500000);
}

void LogSerial::init(unsigned long _baudrate=500000) {
    static FILE _logOutput = {0};
 
    _logSerial=getInstance();
    _logSerial->begin(_baudrate);
    fdev_setup_stream(&_logOutput, logSerial_Putc, NULL, _FDEV_SETUP_WRITE);
    stdout = stderr = &_logOutput;
}

size_t LogSerial::LOG_SERIAL_PRINT_METHOD_NAME (const char *fmt, ...) {
    va_list vp;
    int i;

    va_start(vp, fmt);
	i = vfprintf(stdout, fmt, vp);
	va_end(vp);

	return i;
}

size_t LogSerial::LOG_SERIAL_PRINT_METHOD_NAME (const __FlashStringHelper *fmt, ...) {
    va_list vp;
    int i;

    va_start(vp, fmt);
 	stdout->flags |= __SPGM;// using prog_mem
	i = vfprintf_P(stdout, reinterpret_cast<const char *>(fmt), vp);
	stdout->flags &= ~__SPGM;
	va_end(vp);

	return i;
}
