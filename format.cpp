//
// format.cpp -- generic formatting routines for longs and doubles
//
// (c) 2005 Dr. Andreas Mueller, Beratung und Entwicklung
//
#include <format.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <alloca.h>

namespace powermeter {

//
// the stringprintf function creates a std::string from a format str
// and some variable arguments
//
#define	BUFFERSIZE	1024
std::string vstringprintf(const char *format, va_list ap) {
	char *buffer;		// should be sufficient in this application
	int bytes;
	buffer = (char *)alloca(BUFFERSIZE);	// very small buffer
	bytes = vsnprintf(buffer, BUFFERSIZE, format, ap);
	if (bytes >= BUFFERSIZE) {
		// truncated, try again with the recommended buffer size
		buffer = (char *) alloca(bytes + 1);
		vsnprintf(buffer, bytes + 1, format, ap);
	}
	return std::string(buffer);
}

std::string stringprintf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	std::string	result = vstringprintf(format, ap);
	va_end(ap);
	return result;
}

} // namespace powermeter
