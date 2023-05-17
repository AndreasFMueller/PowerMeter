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
	int dbport,
	const std::string& stationname, const std::string& sensorname,
	messagequeue& queue)
	: _hostname(hostname), _dbname(dbname), _dbuser(dbuser),
	  _dbpassword(dbpassword), _dbport(dbport),
	  _stationname(stationname), _sensorname(sensorname),
	  _queue(queue) {
	// create database connection
	_mysql = mysql_init(NULL);
	if (NULL == mysql_real_connect(_mysql, _hostname.c_str(),
		_dbuser.c_str(), _dbpassword.c_str(), _dbname.c_str(),
		_dbport, NULL, 0)) {
		mysql_close(_mysql);
		_mysql = NULL;
		throw std::runtime_error("cannot open database connection");
	}

	// prepare a statement
	MYSQL_STMT	*stmt = mysql_stmt_init(_mysql);
	if (NULL == stmt) {
		throw std::runtime_error("cannot create stmt");
	}

	// prepare the query
	std::string	query(	"select st.id, se.id "
				"from station st, sensor se "
				"where se.stationid = st.id "
				"  and st.name = ?"
				"  and se.name = ?");
	int	rc = mysql_stmt_prepare(stmt, query.c_str(), query.size());
	if (0 != rc) {
		mysql_stmt_close(stmt);
		throw std::runtime_error("cannot prepare statement");
	}

	// create bind structure
	MYSQL_BIND	parameters[2];
	memset(parameters, 0, sizeof(parameters));

	char	stationbuffer[_stationname.size() + 1];
	strcpy(stationbuffer, _stationname.c_str());
	unsigned long	stationlength = _stationname.size() + 1;
	parameters[0].buffer = stationbuffer;
	parameters[0].buffer_length = _stationname.size() + 1;
	parameters[0].length = &stationlength;
	parameters[0].buffer_type = MYSQL_TYPE_STRING;

	char	sensorbuffer[_sensorname.size() + 1];
	strcpy(stationbuffer, _sensorname.c_str());
	unsigned long	sensorlength = _sensorname.size() + 1;
	parameters[1].buffer = sensorbuffer;
	parameters[1].buffer_length = _sensorname.size() + 1;
	parameters[1].length = &sensorlength;
	parameters[1].buffer_type = MYSQL_TYPE_STRING;

	// bind the parameters
	rc = mysql_stmt_bind_param(stmt, parameters);
	if (0 != rc) {
		mysql_stmt_close(stmt);
		throw std::runtime_error("cannot bind");
	}

	// execute the statement
	rc = mysql_stmt_execute(stmt);
	if (0 != rc) {
		mysql_stmt_close(stmt);
		throw std::runtime_error("cannot execute");
	}

	// bind the result fields
	MYSQL_BIND	results[2];
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
		mysql_stmt_close(stmt);
		throw std::runtime_error("cannot bind result parameters");
	}

	// retrieve the data
	rc = mysql_stmt_fetch(stmt);
	if (0 != rc) {
		mysql_stmt_close(stmt);
		throw std::runtime_error("cannot retrieve ids");
	}

	// cleanup
	mysql_stmt_close(stmt);

	// get the field information
	query = std::string("select name, id from mfield from mfield");
	mysql_store_result(_mysql);
	if (mysql_query(_mysql, query.c_str())) {
		throw std::runtime_error("cannot retrieve field information");
	}
	MYSQL_RES	*mres = mysql_store_result(_mysql);
	MYSQL_ROW	row;
	while (NULL != (row = mysql_fetch_row(mres))) {
		std::string	name = std::string(row[0]);
		int	id = std::stoi(row[1]);
		_fields.insert(std::make_pair(name, id));
	}
	mysql_free_result(mres);

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
		mysql_stmt_close(stmt);
		throw std::runtime_error("cannot prepare the statement");
	}

	// bind the parameters
	MYSQL_BIND	parameters[4];
	memset(parameters, 0, sizeof(parameters));
	long long	timekey = std::chrono::duration_cast<std::chrono::seconds>(m.when().time_since_epoch()).count();
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
		throw std::runtime_error("cannot bind parameters");
	}

	// go through the message
	for (auto i = m.begin(); i != m.end(); i++) {
		fid = fieldid(i->first);
		value = i->second;
		rc = mysql_stmt_execute(stmt);
		if (0 != rc) {
			mysql_stmt_close(stmt);
			throw std::runtime_error("execute failed");
		}
	}
	
	// cleanup
	mysql_stmt_close(stmt);
}

void	database::run() {
	while (_active) {
		message	m = _queue.extract();
		// send the message to the database
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
