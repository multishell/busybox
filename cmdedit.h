#ifndef CMDEDIT_H
#define CMDEDIT_H

void cmdedit_init(void);
void cmdedit_terminate(void);
void cmdedit_read_input(char* promptStr, char* command);		/* read a line of input */

#endif /* CMDEDIT_H */
