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

namespace powermeter {

static struct option	longopts[] = {
{ "debug",		no_argument,		NULL,		'd' },
{ "dbhostname",		required_argument,	NULL,		'H' },
{ "dbname",		required_argument,	NULL,		'D' },
{ "dbuser",		required_argument,	NULL,		'U' },
{ "dbpassword",		required_argument,	NULL,		'P' },
{ "meterhostname",	required_argument,	NULL,		'm' },
{ "meterport",		required_argument,	NULL,		'p' },
{ "meterid",		required_argument,	NULL,		'i' },
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
	std::string	meterhostname;
	unsigned short	meterport;
	unsigned char	meterid;
	bool	foreground = false;

	// read parameters from the command line
	while (EOF != (c = getopt_long(argc, argv, "dH:D:U:P:m:p:i:V",
		longopts, NULL)))
		switch (c) {
		case 'd':
			debuglevel = 1;
			break;
		case 'H':
			dbhostname = std::string(optarg);
			break;
		case 'D':
			dbname = std::string(optarg);
			break;
		case 'U':
			dbuser = std::string(optarg);
			break;
		case 'P':
			dbpassword = std::string(optarg);
			break;
		case 'm':
			meterhostname = std::string(optarg);
			break;
		case 'p':
			meterport = std::stoi(optarg);
			break;
		case 'i':
			meterid = std::stoi(optarg);
			break;
		case 'f':
			foreground = true;
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
	database	db(dbhostname, dbname, dbuser, dbpassword, queue);
	
	// create the source, i.e. the thread reading from the power meter
	meter	mtr(meterhostname, meterport, meterid, queue);

	// wait for the message queue to report a problem
	queue.wait(std::chrono::seconds(120));

	return EXIT_SUCCESS;
}

} // namespace powermeter

int	main(int argc, char *argv[]) {
	try {
		return powermeter::main(argc, argv);
	} catch (const std::exception& x) {

	} catch (...) {
		
	}
}
