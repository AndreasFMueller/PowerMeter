/*
 * debug.c -- debugging subsystem
 *
 * (c) 2002 Dr. Andreas Mueller, Beratung und Entwicklung
 *
 * $Id: debug.c 5267 2009-07-07 07:41:56Z afm $
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <debug.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

/**
 @file
 @brief In&Work debug subsystem implementation
 */

int	debuglevel = 0;
int	debugmaxsize = -1;

#define	MSGSIZE	10240

/**
 File pointer for the log file, only used if logging goes to a file
 */
static FILE	*logfile = NULL;
/**
 File name for the log file, must be remembered to be able to automatically
 rotate log files
 */
static char	*logfilename = NULL;
/**
 Identifier given to all log messages. Used as a parameter to openlog if logging goes
 to syslog.
 */
static char	*logident = NULL;
/**
 Time stamp format string for log messages to files (syslog has its own method
 to format time stamps.
 */
static char	*logformat = NULL;

/**
 @brief Holder class for log facility name mapping.

 The logfacility_s structure is used to map facility names to syslog facility
 codes.
 */
struct logfacility_s {
	/**
	 Name of the facility
	 */
	const char	*name;
	/**
	 Syslog facility code.
	 */
	int		facility;
};
/**
 Syslog usually has 17 known log facilities.
 */
#define	NFACILITIES	17
/**
 @brief The lf array contains the names for the known log facilities.
 It is used by the debug_setup() function to convert a syslog URL
 into arguments to the openlog call. The defined log facilities are
 auth, cron, daemon, kern, lpr, mail, news, user, uucp, local0, local1,
 local2, local3, local4, local5, local6, local7
 @hideinitializer
 */
static struct logfacility_s	lf[NFACILITIES] = {
	{	"auth",		LOG_AUTH	},
	{	"cron",		LOG_CRON	},
	{	"daemon",	LOG_DAEMON	},
	{	"kern",		LOG_KERN	},
	{	"lpr",		LOG_LPR		},
	{	"mail",		LOG_MAIL	},
	{	"news",		LOG_NEWS	},
	{	"user",		LOG_USER	},
	{	"uucp",		LOG_UUCP	},
	{	"local0",	LOG_LOCAL0	},
	{	"local1",	LOG_LOCAL1	},
	{	"local2",	LOG_LOCAL2	},
	{	"local3",	LOG_LOCAL3	},
	{	"local4",	LOG_LOCAL4	},
	{	"local5",	LOG_LOCAL5	},
	{	"local6",	LOG_LOCAL6	},
	{	"local7",	LOG_LOCAL7	}
};

void	debug_set_id(const char *ident) {
	/* if the logident is already set, we have to free it to	*/
	/* prevent a memory leak					*/
	if (NULL != logident) {
		free(logident); logident = NULL;
	}
	if (NULL != ident) {
		logident = strdup(ident);
	}
}
static void	debug_setup_filename(const char *lfn) {
	logfile = fopen(lfn, "a");
	if (NULL != logfile) {
		logfilename = strdup(lfn);
	}
}
void	debug_set_logformat(const char *lformat) {
	if (NULL != logformat) {
		free(logformat); logformat = NULL;
	}
	if (NULL != lformat) {
		logformat = strdup(lformat);
	}
}
static void	debug_cleanup_file() {
	/* if the logfile is already open, close it first		*/
	if (NULL != logfile) {
		if (logfile != stderr) {
			fclose(logfile);
		}
		logfile = NULL;
		free(logfilename);
		logfilename = NULL;
	}
}

int	debug_setup_file(const char *ident, const char *logfilename) {
	debug_set_id(ident);
	debug_setup_filename(logfilename);
	return 0;
}

int	debug_setup(const char *ident, const char *logurl) {
	int		i;
	const char	*logfacility;
	const char	*lfn = NULL;

	/* remember the identifier					*/
	debug_set_id(ident);

	/* cleanup in case we had a log file open			*/
	debug_cleanup_file();

	/* handle case of syslog urls					*/
	if (0 == strncmp("syslog:", logurl, 7)) {
		logfacility = logurl + 7;
		for (i = 0; i < NFACILITIES; i++) {
			if (0 == strcmp(logfacility, lf[i].name)) {
				openlog(ident, LOG_PID, lf[i].facility);
				return 0;
			}
		}
	}

	/* handle explicit file:// urls 				*/
	lfn = logurl;
	if (0 == strncmp("file://", logurl, 7)) {
		if (strcmp("/-", logurl + 7) == 0) {
			logfile = stderr;
			return 0;
		}
		lfn = logurl + 7;
	}

	/* handle the rest, it must be a file				*/
	logfile = stderr;
	debug_setup_filename(lfn);
	if (logfile != NULL) {
		return 0; /* if we can open the file, everything is ok  */
	} else {
		logfile = stderr;
	}

	/* if we don't get to this point, then either the log url is 	*/
	/* illegal, or the facility does not exist			*/
	errno = ESRCH;
	return -1;
}

/**
 @brief Rotate the log file if the the maximum log size is exceeded.

 If log messages are sent to a file, and if the current log file is at least
 debugmaxsize bytes long, the current log file is closed, renamed by attaching
 the suffix provided as an argument, and a new log file with the original name
 is opened.
 
 This function has no effect if log messages are not sent to a log file or if
 the debugmaxsize is negative.
 
 @param suffix  suffix to append to the log file when rotating.
 */
static void	debug_rotate(const char *suffix) {
	struct stat	sb;
	char	name[1024];
	/* rotate can only happen if we have a logfile and the size	*/
	/* limit is enabled						*/
	if ((NULL == logfilename) || (NULL == logfile) || (debugmaxsize < 0)) {
		return;
	}

	/* check whether the log file has grown too large		*/
	if (fstat(fileno(logfile), &sb) < 0) {
		fprintf(stderr, "%s warning: cannot stat logfile\n",
			(NULL == logident) ? "debug" : logident);
		return;
	}
	if (sb.st_size < debugmaxsize) 
		return;

	/* need to rotate the log					*/
	fclose(logfile);
	snprintf(name, sizeof(name), "%s%s", logfilename, suffix);
	if (rename(logfilename, name) < 0) {
		fprintf(stderr, "cannot rename logfile to %s: %s\n", name,
			strerror(errno));
	}
	logfile = fopen(logfilename, "a");
}

static void	_vdebug(int loglevel, const char *file, int line, int flags,
	const char *format, va_list ap) {
	char		msgbuffer[MSGSIZE], msgbuffer2[MSGSIZE],
			prefix[MSGSIZE], tstp[MSGSIZE];
	time_t		t;
	struct tm	*tmp;
	int		localerrno;

	/* find out whether logging is necessary:
	   no log messages are sent
		- if the maximum debug log size is 0
		- if the log level of the message is larger than LOG_DEBUG and
		  the debuglevel is 0
	 */
	if (debugmaxsize == 0) {
		return;
	}
	if (loglevel >= (LOG_DEBUG + debuglevel)) {
		return;
	}

	/* format the message content					*/
	localerrno = errno;
	vsnprintf(msgbuffer2, sizeof(msgbuffer2), format, ap);
	if (flags & DEBUG_ERRNO)
		snprintf(msgbuffer, sizeof(msgbuffer), "%s: %s (%d)",
			msgbuffer2, strerror(localerrno), localerrno);
	else
		strcpy(msgbuffer, msgbuffer2);

	/* add prefix depending on file or syslog, and send to dest	*/
	if (logfile != NULL) {
		/* compute a time stamp					*/
		t = time(NULL);
		tmp = localtime(&t);
		strftime(tstp, sizeof(tstp),
			(NULL != logformat) ? logformat : "%b %e %H:%M:%S", tmp);

		/* compute the format prefix				*/
		if (flags & DEBUG_NOFILELINE)
			snprintf(prefix, sizeof(prefix), "%s %s[%d]:",
				tstp, (logident) ? logident : "(unknown)",
				getpid());
		else
			snprintf(prefix, sizeof(prefix), "%s %s[%d] %s:%03d:",
				tstp, (logident) ? logident : "(unknown)",
				getpid(), file, line);

		/* write the message to the logfile			*/
		fprintf(logfile, "%s %s\n", prefix, msgbuffer);
		fflush(logfile);
	} else {
		if (DEBUG_NOFILELINE & flags)
			syslog((loglevel > LOG_DEBUG) ? LOG_DEBUG : loglevel,
				"%s", msgbuffer);
		else
			syslog((loglevel > LOG_DEBUG) ? LOG_DEBUG : loglevel,
				"%s:%03d: %s",
				(file) ? file : "(file unknown)", line,
				msgbuffer);
	}

	/* check whether we should rotate the logfile			*/
	debug_rotate(".old");
}

void	debug(int loglevel, const char *file, int line, int flags,
	const char *format, ...) {
	va_list	ap;
	va_start(ap, format);
	vdebug(loglevel, file, line, flags, format, ap);
	va_end(ap);
}

void	vdebug(int loglevel, const char *file, int line, int flags,
	const char *format, va_list ap) {
	_vdebug(loglevel, file, line, flags, format, ap);
}

