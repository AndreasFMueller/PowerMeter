/*
 * powermeterd.cpp -- power meter main method
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <cstdlib>
#include <cstdio>
#include <getopt.h>
#include <stdexcept>
#include <message.h>
#include <database.h>
#include <meter.h>
#include <debug.h>
#include <sys/stat.h>
#include <syslog.h>

namespace powermeter {

static struct option	longopts[] = {
{ "debug",		no_argument,		NULL,		'd' },
{ "dbhostname",		required_argument,	NULL,		'H' },
{ "dbname",		required_argument,	NULL,		'D' },
{ "dbuser",		required_argument,	NULL,		'U' },
{ "dbpassword",		required_argument,	NULL,		'P' },
{ "dbport",		required_argument,	NULL,		'Q' },
{ "meterhostname",	required_argument,	NULL,		'm' },
{ "meterport",		required_argument,	NULL,		'p' },
{ "meterid",		required_argument,	NULL,		'i' },
{ "stationname",	required_argument,	NULL,		'S' },
{ "sensorname",		required_argument,	NULL,		's' },
{ "version",		no_argument,		NULL,		'V' },
{ "foreground",		no_argument,		NULL,		'f' },
{ NULL,			0,			NULL,		 0  }
};

/**
 * \brief Main method for the powermeter
 *
 * \param argc		number of arguments
 * \param argv		argument strings
 */
int	main(int argc, char *argv[]) {
	int	c;
	std::string	dbhostname;
	std::string	dbname;
	std::string	dbuser;
	std::string	dbpassword;
	int		dbport = 3307;
	std::string	stationname;
	std::string	sensorname;
	std::string	meterhostname;
	unsigned short	meterport;
	unsigned char	meterid;
	bool	foreground = false;
	debug_setup("powermeterd", "file:///-");
	debuglevel = LOG_DEBUG;

	// read parameters from the command line
	while (EOF != (c = getopt_long(argc, argv, "dH:D:U:P:Q:S:s::m:p:i:V",
		longopts, NULL)))
		switch (c) {
		case 'd':
			debuglevel = LOG_DEBUG;
			break;
		case 'H':
			dbhostname = std::string(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db hostname: %s",
				dbhostname.c_str());
			break;
		case 'D':
			dbname = std::string(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db name: %s",
				dbname.c_str());
			break;
		case 'U':
			dbuser = std::string(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db user: %s",
				dbuser.c_str());
			break;
		case 'P':
			dbpassword = std::string(optarg);
			break;
		case 'Q':
			dbport = std::stoi(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db port: %d", dbport);
			break;
		case 'S':
			stationname = std::string(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "stationname: %s",
				stationname.c_str());
			break;
		case 's':
			sensorname = std::string(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "sensorname: %s",
				sensorname.c_str());
			break;
		case 'm':
			meterhostname = std::string(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "meter hostname: %s",
				meterhostname.c_str());
			break;
		case 'p':
			meterport = std::stoi(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "meterport: %d",
				meterport);
			break;
		case 'i':
			meterid = std::stoi(optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "meterid: %d", meterid);
			break;
		case 'f':
			foreground = true;
			debug(LOG_DEBUG, DEBUG_LOG, 0,
				"running in the foreground");
			break;
		case 'V':
			// XXX show version
			break;
		}

	// if not running in the foreground, daemonize now
	if (!foreground) {
		pid_t	pid = fork();
		if (pid < 0) {
			// XXX cannot fork
			return EXIT_FAILURE;
		}
		if (pid > 0) {
			return EXIT_SUCCESS;
		}
		setsid();
		chdir("/");
		umask(0);
	}

	// create the queue
	messagequeue	queue;

	// create the destination, i.e. the thread writing into the database
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start the database");
	database	db(dbhostname, dbname, dbuser, dbpassword, dbport,
				stationname, sensorname, queue);
	
	// create the source, i.e. the thread reading from the power meter
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start the meter");
	meter	mtr(meterhostname, meterport, meterid, queue);

	sleep(10);

	// wait for the message queue to report a problem
	debug(LOG_DEBUG, DEBUG_LOG, 0, "waiting for queue event");
	queue.wait(std::chrono::seconds(120));

	return EXIT_SUCCESS;
}

} // namespace powermeter

int	main(int argc, char *argv[]) {
	try {
		return powermeter::main(argc, argv);
	} catch (const std::exception& x) {
		debug(LOG_ERR, DEBUG_LOG, 0, "powermeter main failed: %s",
			x.what());
	} catch (...) {
		debug(LOG_ERR, DEBUG_LOG, 0, "powermeter main failed");
	}
}
