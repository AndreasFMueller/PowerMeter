/*
 * database.cpp
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <database.h>
#include <mysql.h>
#include <format.h>
#include <debug.h>
#include <cstring>

namespace powermeter {

database::database(const configuration& config,
	messagequeue& queue)
	: _hostname(config.stringvalue("dbhostname")),
	  _dbname(config.stringvalue("dbname")),
	  _dbuser(config.stringvalue("dbuser")),
	  _dbpassword(config.stringvalue("dbpassword")),
	  _dbport(config.intvalue("dbport")),
	  _stationname(config.stringvalue("stationname")),
	  _sensorname(config.stringvalue("sensorname")),
	  _queue(queue) {
	// create database connection
	_mysql = mysql_init(NULL);
	if (NULL == mysql_real_connect(_mysql, _hostname.c_str(),
		_dbuser.c_str(), _dbpassword.c_str(), _dbname.c_str(),
		_dbport, NULL, 0)) {
		mysql_close(_mysql);
		_mysql = NULL;
		std::string	msg("cannot open database connection");
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// prepare a statement
	MYSQL_STMT	*stmt = mysql_stmt_init(_mysql);
	if (NULL == stmt) {
		std::string	msg = stringprintf("cannot create stmt: %s",
			mysql_error(_mysql));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// prepare the query
	std::string	query(	"select st.id, se.id "
				"from station st, sensor se "
				"where se.stationid = st.id "
				"  and st.name = ?"
				"  and se.name = ?"
				" ");
	int	rc = mysql_stmt_prepare(stmt, query.c_str(), query.size());
	if (0 != rc) {
		std::string	msg = stringprintf("cannot prepare query '%s': "
			"%s", query.c_str(), mysql_stmt_error(stmt));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		mysql_stmt_close(stmt);
		throw std::runtime_error(msg);
	}

	// create bind structure
	MYSQL_BIND	parameters[2];
	memset(parameters, 0, sizeof(parameters));

	char	stationbuffer[_stationname.size() + 1];
	strcpy(stationbuffer, _stationname.c_str());
	unsigned long	stationlength = _stationname.size();
	parameters[0].buffer = stationbuffer;
	parameters[0].buffer_length = stationlength + 1;
	parameters[0].length = &stationlength;
	parameters[0].buffer_type = MYSQL_TYPE_VAR_STRING;
	debug(LOG_DEBUG, DEBUG_LOG, 0, "station name: '%s'",
		parameters[0].buffer);

	char	sensorbuffer[_sensorname.size() + 1];
	strcpy(sensorbuffer, _sensorname.c_str());
	unsigned long	sensorlength = _sensorname.size();
	parameters[1].buffer = sensorbuffer;
	parameters[1].buffer_length = sensorlength + 1;
	parameters[1].length = &sensorlength;
	parameters[1].buffer_type = MYSQL_TYPE_VAR_STRING;
	debug(LOG_DEBUG, DEBUG_LOG, 0, "sensor name: '%s'",
		parameters[1].buffer);

	// bind the parameters
	debug(LOG_DEBUG, DEBUG_LOG, 0, "binding parameters");
	rc = mysql_stmt_bind_param(stmt, parameters);
	if (0 != rc) {
		std::string	msg = stringprintf("cannot bind: %s",
			mysql_stmt_error(stmt));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		mysql_stmt_close(stmt);
		throw std::runtime_error(msg);
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "bind complete");

	// execute the statement
	rc = mysql_stmt_execute(stmt);
	if (0 != rc) {
		std::string	msg = stringprintf("cannot execute: %s",
			mysql_stmt_error(stmt));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		mysql_stmt_close(stmt);
		throw std::runtime_error(msg);
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "station '%s' %d, sensor '%s' %d",
		_stationname.c_str(), _stationid,
		_sensorname.c_str(), _sensorid);

	// bind the result fields
	MYSQL_BIND	results[2];
	memset(results, 0, sizeof(results));
	memset(&_stationid, 0, sizeof(_stationid));
	results[0].buffer = &_stationid;
	results[0].buffer_length = sizeof(_stationid);
	results[0].buffer_type = MYSQL_TYPE_TINY;
	memset(&_sensorid, 0, sizeof(_sensorid));
	results[1].buffer = &_sensorid;
	results[1].buffer_length = sizeof(_sensorid);
	results[1].buffer_type = MYSQL_TYPE_TINY;
	rc = mysql_stmt_bind_result(stmt, results);
	if (0 != rc) {
		std::string	msg = stringprintf("cannot bind result: %s",
			mysql_stmt_error(stmt));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		mysql_stmt_close(stmt);
		throw std::runtime_error(msg);
	}

	// retrieve the data
	rc = mysql_stmt_fetch(stmt);
	if (0 != rc) {
		std::string	msg = stringprintf("cannot retrieve ids: %s",
			mysql_stmt_error(stmt));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		mysql_stmt_close(stmt);
		throw std::runtime_error(msg);
	}

	// cleanup
	mysql_stmt_close(stmt);

	// get the field information
	query = std::string("select name, id from mfield");
	mysql_store_result(_mysql);
	if (mysql_query(_mysql, query.c_str())) {
		std::string	msg = stringprintf("cannot retrieve field "
			"information: %s", mysql_error(_mysql));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	MYSQL_RES	*mres = mysql_store_result(_mysql);
	MYSQL_ROW	row;
	while (NULL != (row = mysql_fetch_row(mres))) {
		std::string	name = std::string(row[0]);
		int	id = std::stoi(row[1]);
		_fields.insert(std::make_pair(name, id));
		debug(LOG_DEBUG, DEBUG_LOG, 0, "'%s' -> %d", name.c_str(), id);
	}
	mysql_free_result(mres);

	// launch the thread
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
		debug(LOG_DEBUG, DEBUG_LOG, 0, "launch database thread");
		d->run();
		debug(LOG_DEBUG, DEBUG_LOG, 0, "database thread terminates");
	} catch (const std::exception& x) {
		debug(LOG_ERR, DEBUG_LOG, 0, "database thread fails with "
			"exception %s", x.what());
	} catch (...) {
		debug(LOG_ERR, DEBUG_LOG, 0, "database thread fails");
	}
}

void	database::store(const message& m) {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "storing a new message");
	// prepare a statment
	MYSQL_STMT	*stmt = mysql_stmt_init(_mysql);
	if (NULL == stmt) {
		throw std::runtime_error("cannot construct a statement");
	}
	std::string	query(
		"insert into sdata(timekey, sensorid, fieldid, value) "
		"values (?, ?, ?, ?)");
	int	rc = mysql_stmt_prepare(stmt, query.c_str(), query.size());
	if (0 != rc) {
		std::string	msg = stringprintf("cannot prepare statement "
			"'%s': %s", query.c_str(), mysql_error(_mysql));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		mysql_stmt_close(stmt);
		throw std::runtime_error(msg);
	}

	// bind the parameters
	MYSQL_BIND	parameters[4];
	memset(parameters, 0, sizeof(parameters));
	long long	timekey = std::chrono::duration_cast<std::chrono::seconds>(m.when().time_since_epoch()).count();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "timekey = %ld", timekey);
	parameters[0].buffer = &timekey;
	parameters[0].buffer_type = MYSQL_TYPE_LONGLONG;
	parameters[1].buffer = &_sensorid;
	parameters[1].buffer_type = MYSQL_TYPE_TINY;
	char	fid;
	parameters[2].buffer = &fid;
	parameters[2].buffer_type = MYSQL_TYPE_TINY;
	float	value;
	parameters[3].buffer = &value;
	parameters[3].buffer_type = MYSQL_TYPE_FLOAT;
	rc = mysql_stmt_bind_param(stmt, parameters);
	if (0 != rc) {
		std::string	msg = stringprintf("cannot bind: %s",
			mysql_stmt_error(stmt));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "parameters bound");

	// go through the message
	for (auto i = m.begin(); i != m.end(); i++) {
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "store value for %s",
		//	i->first.c_str());
		fid = fieldid(i->first);
		value = i->second;
		rc = mysql_stmt_execute(stmt);
		if (0 != rc) {
			std::string	msg = stringprintf("execute failed: %s",
				mysql_stmt_error(stmt));
			debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
			mysql_stmt_close(stmt);
			throw std::runtime_error(msg);
		}
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "all values stored");
	
	// cleanup
	mysql_stmt_close(stmt);
}

void	database::run() {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "running database thread");
	while (_active) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "waiting for message");
		message	m = _queue.extract();
		// send the message to the database
		debug(LOG_DEBUG, DEBUG_LOG, 0, "storing message");
		store(m);
	}
}

char	database::fieldid(const std::string& fieldname) const {
	auto	i = _fields.find(fieldname);
	if (i == _fields.end()) {
		throw std::runtime_error("field name not found");
	}
	return i->second;
}

} // namespace powermeter
