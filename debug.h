/*
 * debug.h -- debugging subsystem
 *
 * (c) 2002 Dr. Andreas Mueller, Beratung und Entwicklung
 *
 * $Id: debug.h 5267 2009-07-07 07:41:56Z afm $
 */
#ifndef _debug_h
#define _debug_h

/*
#include "includes.h"
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 @file
 @brief debug subsystem definitions
 */

/**
 If this flag is specified in the flags variable of a call to debug(),
 no file and line number information is included in the log message.
 Of course, the filename and line number arguments still have to be provided
 to satisfy the prototype.
 */
#define	DEBUG_NOFILELINE	1
/**
 If the DEBUG_ERRNO flag is inclued in a call to debug(), then the error
 number and error description associated with the current value of errno
 is included in the message.
 */
#define	DEBUG_ERRNO		2
/**
 As a convenience to the programmer, the DEBUG_LOG macro is provided to
 slightly shorten calls to debug.
 */
#define	DEBUG_LOG		__FILE__, __LINE__

/**
 @brief debug level for debugging of In&Work programs
 
 If debuglevel is zero, then all messages with a syslog priority (loglevel)
 >= LOG_DEBUG are suppressed. If debuglevel is greater than 0, all messages
 with priority < LOG_DEBUG + debuglevel are sent.
 */
extern int	debuglevel;
/**
 @brief Maximum size of a debug message file.

 Setting this variable to a value > 0 limits the size of the log file to
 a length close to this value. After a message has been appended to the
 log file, the library checks whether the log file has grown beyond the
 size allowed by debugmaxsize, and rotates the log file to a file of the
 same name with an extension .old appended.
 
 If debugmaxsize is set to 0, then logging is suppressed entirely, even
 if messages would only be sent to syslog.

 The default value is -1, which causes no size checking of the log file.
 */
extern int	debugmaxsize;
/**
 @brief Initialize debuging.

 Initially all log messages are sent to the syslog daemon, with a facility and
 identifier determined by the defaults of the syslog function of the platform.
 
 This method should be used to modify the identifier to be used within log
 messages and the destination where log messages will be sent. An URL-like
 specification is used to specify the log destination. To send messages to
 a file, use an ordinary file URL like file:///path/to/logfile. To send
 log messages to a syslog facility of your choice, use the format
 syslog:facility.
 
 The logging system can be reconfigured at any time.
 @param ident	Identifier string to include in all debug messages
 @param logurl	Url specifying debug log destination
 @return	Indicates success or failure of the operation. If the operation
		is successful, 0 is returned. If the the logurl
		points to a file, and the file cannot be opened for writing, then
		-1 is returned.
 */
extern int	debug_setup(const char *ident, const char *logurl);
/**
 @brief Initialize debugging to a file.
 @param ident	Identifier string to include in all debug messages.
 @param logfilename	The name of a file to which log messages should be
			appended.
 @return	Returns 0 if the file can be opened for writing, and -1 if opening
		the file fails.
 */
extern int	debug_setup_file(const char *ident, const char *logfilename);
/**
 @brief Set logging identifier.
 @param ident	Identifier string to include in all debug messages.
 */
extern void	debug_set_id(const char *ident);
/**
 @brief Set logging timestamp format
 @param logformat	String as documented for strftime to be used to format
			time stamp of the log messages. Supported timestamps depend
			on the platform.
 */
extern void	debug_set_logformat(const char *logformat);
/**
 @brief Format a log message.
 This function is normally used to format a log message and send it at a
 given log level. Note that in addition to the log levels defined by
 @param	loglevel	Message level. If the message level exceeds the 
			value of debuglevel, the message is printed, otherwise
			it is suppressed.
 @param filename	Filename to log for this messages, usually __FILE__
 @param line	Line number to log for this message, usually __LINE__
 @param flags	The flags parameter is a bit map obtained by or-ing together
		some of the available flags. Tehese are DEBUG_NOFILELINE
		and DEBUG_ERRNO.
 @param format	printf()-like  format string
 */
extern void	debug(int loglevel, const char *filename, int line, int flags,
			const char *format, ...);
/**
 @brief format a log message
 @param	loglevel	Message level. If the message level exceeds the 
			value of debuglevel, the message is printed, otherwise
			it is suppressed.
 @param filename	filename to log for this messages, usually __FILE__
 @param line	Line number to log for this message, usually __LINE__
 @param flags	flags
 @param format	printf()-like  format string
 @param ap	variable argument list for arguments specified in the format
		argument
 */
extern void	vdebug(int loglevel, const char *filename, int line, int flags,
			const char *format, va_list ap);

#ifdef __cplusplus
}
#endif

#endif /* _debug_h */
