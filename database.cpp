/*
 * database.cpp
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <database.h>
#include <mysql.h>

namespace powermeter {

database::database(const std::string& hostname, const std::string& dbname,
	const std::string& dbuser, const std::string& dbpassword,
	messagequeue& queue)
	: _hostname(hostname), _dbname(dbname), _dbuser(dbuser),
	  _dbpassword(dbpassword), _queue(queue) {
	// XXX create database connection

	// XXX create array of identifiers for the data to be written
	// XXX to the database

	// XXX launch the thread
	std::unique_lock<std::mutex>	lock(_mutex);
	_active = true;
	_thread = std::thread(database::launch, this);
}

database::~database() {
	_active = false;
	_signal.notify_all();
	if (_thread.joinable()) {
		_thread.join();
	}
}

void	database::launch(database *d) {
	try {
		d->run();
	} catch (const std::exception& x) {
		// XXX report database error exception
	} catch (...) {
		// XXX report database error
	}
}

void	database::store(const message& m) {
	// XXX perform storing the data
}

void	database::run() {
	while (_active) {
		message	m = _queue.extract();
		// send the message to the database
		store(m);
	}
}

} // namespace powermeter
