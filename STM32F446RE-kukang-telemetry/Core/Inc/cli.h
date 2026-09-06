#ifndef __CLI_H
#define __CLI_H

#include "main.h"

void CLI_Init(void);
void CLI_ProcessChar(char c);
void CLI_Task(void);
void CLI_Print(const char *format, ...);

#endif // __CLI_H
