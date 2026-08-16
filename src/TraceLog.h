//
//  TraceLog.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.08.26.
//

#pragma once

typedef enum {
    LOG_ALL = 0,        // Display all logs
    LOG_TRACE,          // Trace logging, intended for internal use only
    LOG_DEBUG,          // Debug logging, used for internal debugging, it should be disabled on release builds
    LOG_INFO,           // Info logging, used for program execution info
    LOG_WARNING,        // Warning logging, used on recoverable failures
    LOG_ERROR,          // Error logging, used on unrecoverable failures
    LOG_FATAL,          // Fatal logging, used to abort program: exit(EXIT_FAILURE)
    LOG_NONE            // Disable logging
} TraceLogLevel;

void TraceLog(int logType, const char *text, ...);

#define TRACELOG(level, ...) TraceLog(level, __VA_ARGS__)
