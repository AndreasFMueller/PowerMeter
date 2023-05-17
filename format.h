//
// format.h -- format numbers as std::string
//
#ifndef _format_h
#define _format_h

#include <string>
#include <cstdarg>

namespace powermeter {

extern std::string stringprintf(const char *format, ...);
extern std::string vstringprintf(const char *format, va_list args);

} // namespace powermeter

#endif	/* _format_h */
