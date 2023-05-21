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
#include <solivia_meter.h>
#include <modbus_meter.h>
#include <meterfactory.h>
#include <debug.h>
#include <sys/stat.h>
#include <syslog.h>
#include <configuration.h>
#include <iostream>
#include <config.h>
#include <unistd.h>
#include <atomic>

namespace powermeter {

static struct option	longopts[] = {
{ "config",		required_argument,	NULL,		'c' },
{ "debug",		no_argument,		NULL,		'd' },
{ "dbhostname",		required_argument,	NULL,		'H' },
{ "dbname",		required_argument,	NULL,		'D' },
{ "dbuser",		required_argument,	NULL,		'U' },
{ "dbpassword",		required_argument,	NULL,		'P' },
{ "dbport",		required_argument,	NULL,		'Q' },
{ "help",		no_argument,		NULL,		'?' },
{ "metertype",		required_argument,	NULL,		't' },
{ "meterhostname",	required_argument,	NULL,		'm' },
{ "meterport",		required_argument,	NULL,		'p' },
{ "meterid",		required_argument,	NULL,		'i' },
{ "stationname",	required_argument,	NULL,		'S' },
{ "sensorname",		required_argument,	NULL,		's' },
{ "version",		no_argument,		NULL,		'V' },
{ "foreground",		no_argument,		NULL,		'f' },
{ "simulate",		no_argument,		NULL,		'x' },
{ NULL,			0,			NULL,		 0  }
};

static void	usage(const char *progname) {
	std::cout << progname << " [ options ]" << std::endl;
	std::cout << std::endl;
	std::cout << "program to read data from a SAIA Burgess power meter" << std::endl;
	std::cout << std::endl;
	std::cout << "options:" << std::endl;
}

/**
 * \brief Main method for the powermeter
 *
 * \param argc		number of arguments
 * \param argv		argument strings
 */
int	main(int argc, char *argv[]) {
	int	c;
	configuration	config;
	bool	foreground = false;
	debug_setup("powermeterd", "file:///-");
	debuglevel = LOG_DEBUG;

	// read parameters from the command line
	while (EOF != (c = getopt_long(argc, argv, "c:dH:D:U:P:Q:S:s::m:p:i:Vxt:",
		longopts, NULL)))
		switch (c) {
		case 'c':
			config = configuration(optarg);
			break;
		case 'd':
			debuglevel = LOG_DEBUG;
			break;
		case 'H':
			config.set("dbhostname", optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db hostname: %s",
				optarg);
			break;
		case 'D':
			config.set("dbname", optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db name: %s", optarg);
			break;
		case 'U':
			config.set("dbuser", optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db user: %s", optarg);
			break;
		case 'P':
			config.set("dbpassword", optarg);
			break;
		case 'Q':
			config.set("dbport", std::stoi(optarg));
			debug(LOG_DEBUG, DEBUG_LOG, 0, "db port: %d",
				config.intvalue("dbport"));
			break;
		case 'S':
			config.set("stationname", optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "stationname: %s",
				optarg);
			break;
		case 's':
			config.set("sensorname", optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "sensorname: %s",
				optarg);
			break;
		case 'm':
			config.set("meterhostname", optarg);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "meter hostname: %s",
				optarg);
			break;
		case 'p':
			config.set("meterport", std::stoi(optarg));
			debug(LOG_DEBUG, DEBUG_LOG, 0, "meterport: %d",
				config.intvalue("meterport"));
			break;
		case 'i':
			config.set("meterid", std::stoi(optarg));
			debug(LOG_DEBUG, DEBUG_LOG, 0, "meterid: %d",
				config.intvalue("meterid"));
			break;
		case 'f':
			foreground = true;
			debug(LOG_DEBUG, DEBUG_LOG, 0,
				"running in the foreground");
			break;
		case 'x':
			modbus_meter::simulate = true;
			break;
		case 'V':
			std::cout << "powermeter " << VERSION << std::endl;
			return EXIT_SUCCESS;
		case '?':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 't':
			config.set("metertype", optarg);
			break;
		}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "command line read");

	// if not running in the foreground, daemonize now
	if (foreground) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "stay in foreground");
	} else {
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
	database	db(config, queue);
	
	// create the source, i.e. the thread reading from the power meter
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start the meter");
	meterfactory	factory(config);
	std::shared_ptr<meter>	meterp
		= factory.get(config.stringvalue("metertype"), queue);

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
