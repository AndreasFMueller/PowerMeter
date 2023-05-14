/*
 * database.h
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#ifndef _database_h
#define _database_h

#include <message.h>
#include <mysql.h>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace powermeter {

class database {
	// database parameters
	std::string	_hostname;
	std::string	_dbname;
	std::string	_dbuser;
	std::string	_dbpassword;
public:
	const std::string&	hostname() const { return _hostname; }
	const std::string&	dbname() const { return _dbname; }
	const std::string&	dbuser() const { return _dbuser; }
	const std::string&	dbpassword() const { return _dbpassword; }
private:
	// the queue
	messagequeue&	_queue;

	// processing thread
	std::atomic<bool>	_active;
	std::thread		_thread;
	std::mutex		_mutex;
	std::condition_variable	_signal;
public:
	database(const std::string& hostname, const std::string& dbname,
		const std::string& dbuser, const std::string& dbpassword,
		messagequeue& queue);
	~database();
	void	store(const message& m);
	static void	launch(database *d);
	void	run();
};

} // namespace powermeter

#endif /* _database_h */
