/**
 * Initializes the Print sub-system.
 */
void PrintInit(unsigned long speed);

/**
 * Prints a formatted text the serial console.
 * 
 * @param[in] fmt printf-style format string
 */
void print(const char *fmt, ...);

