//
//  TraceLog.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.08.26.
//

#include "TraceLog.h"
#include <iostream>

#define MAX_TRACELOG_MSG_LENGTH      256

void TraceLog(int logType, const char *text, ...)
{
    
    // Message has level below current threshold, don't emit
    //    if ((logType < logTypeLevel) || (text == NULL)) return;
    
    va_list args;
    va_start(args, text);
    
    //    if (traceLog)
    //    {
    //        traceLog(logType, text, args);
    //        va_end(args);
    //        return;
    //    }
    
    char buffer[MAX_TRACELOG_MSG_LENGTH] = { 0 };
    
    switch (logType)
    {
        case LOG_TRACE: memcpy(buffer, "TRACE: ", 7); break;
        case LOG_DEBUG: memcpy(buffer, "DEBUG: ", 7); break;
        case LOG_INFO: memcpy(buffer, "INFO: ", 6); break;
        case LOG_WARNING: memcpy(buffer, "WARNING: ", 9); break;
        case LOG_ERROR: memcpy(buffer, "ERROR: ", 7); break;
        case LOG_FATAL: memcpy(buffer, "FATAL: ", 7); break;
        default: break;
    }
    
    unsigned int textLength = (unsigned int)strlen(text);
    memcpy(buffer + strlen(buffer), text, (textLength < (MAX_TRACELOG_MSG_LENGTH - 12))? textLength : (MAX_TRACELOG_MSG_LENGTH - 12));
    strcat(buffer, "\n");
    vprintf(buffer, args);
    fflush(stdout);
    
    va_end(args);
    
    if (logType == LOG_FATAL) exit(EXIT_FAILURE);  // If fatal logging, exit program
}
