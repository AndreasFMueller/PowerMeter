/*
 * modbus_meter.cpp
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <modbus_meter.h>
#include <debug.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <format.h>
#include <fstream>
#include <algorithm>

namespace powermeter {

/**
 * \brief Parse the field information
 *
 * \param filename	the name of the file containing the information
 */
void	modbus_meter::parsefields(const std::string& filename) {
	std::ifstream	in(filename.c_str());
	char	buffer[1024];
	in.getline(buffer, sizeof(buffer));
	while (!in.eof()) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "parsing: '%s'", buffer);
		if (buffer[0] != '#') {
			modrec_t	record;
			// name
			char	*p = buffer;
			char	*p2 = strchr(p, ',');
			*p2 = '\0';
			record.name = std::string(p);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "name: %s",
				record.name.c_str());
			// unit
			p = p2 + 1;
			p2 = strchr(p, ',');
			*p2 = '\0';
			record.unit = std::stoi(p);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "unit id: %hd",
				record.unit);
			// address
			p = p2 + 1;
			p2 = strchr(p, ',');
			*p2 = '\0';
			record.address = std::stoi(p);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "address: %hd",
				record.address);
			// type
			p = p2 + 1;
			p2 = strchr(p, ',');
			*p2 = '\0';
			std::string	tpname(p);
			record.type = m_uint16;
			if (tpname == "int16") {
				record.type = m_int16;
			}
			if (tpname == "phases") {
				record.type = m_phases;
			}
			debug(LOG_DEBUG, DEBUG_LOG, 0, "type: %d",
				record.type);
			// scalefactor
			p = p2 + 1;
			p2 = strchr(p, ',');
			*p2 = '\0';
			record.scalefactor = std::stof(p);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "scalefactor: %f",
				record.scalefactor);
			// op
			p = p2 + 1;
			std::string	opname(p);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "op: '%s'",
				opname.c_str());
			record.op = m_average;
			if (opname == "min") {
				record.op = m_min;
			}
			if (opname == "max") {
				record.op = m_max;
			}
			if (opname == "signed") {
				record.op = m_signed;
			}
			// store the record
			datatypes.push_back(record);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "added type '%s'",
				record.name.c_str());
		}
		in.getline(buffer, sizeof(buffer));
	}
}


void	modbus_meter::connect_common() {
	// get the ip address of the modbus device
	struct hostent	*hp = gethostbyname(_hostname.c_str());
	if (NULL == hp) {
		std::string	msg = stringprintf("cannot resolve '%s'",
			_hostname.c_str());
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg);
		throw std::runtime_error(msg);
	}

	// check that we have an address
	if (NULL == hp->h_addr) {
		std::string	msg = stringprintf("no address for '%s'",
			_hostname.c_str());
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	
	// convert IP address to string
	struct in_addr	*ia = (struct in_addr *) hp->h_addr_list[0];
	char	*ip = strdup(inet_ntoa(*ia));
	debug(LOG_DEBUG, DEBUG_LOG, 0, "connecting to IP %s", ip);

	// initialize the modbus device
	mb = modbus_new_tcp(ip, _port);
	if (NULL == mb) {
		free(ip);
		std::string	msg = stringprintf("cannot create modbus device");
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	free(ip);

	// connect to the meter
	if (modbus_connect(mb) < 0) {
		debug(LOG_ERR, DEBUG_LOG, 0, "cannot connect to the meter");
		throw std::runtime_error("cannot connect");
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "successfully connected to %s:%d", 
		_hostname.c_str(), _port);
}

void	modbus_meter::connect(const std::string& hostname, int port) {
	_hostname = hostname;
	_port = port;
	connect_common();
}

void	modbus_meter::reconnect() {
	modbus_close(mb);
	modbus_free(mb);
	mb = NULL;
	connect_common();
}

/**
 * \brief Construct a modbus power meter
 *
 * \param config	the configuration to use to get parameters
 * \param queue		the message queue to use to send messages
 */
modbus_meter::modbus_meter(const configuration& config, messagequeue& queue)
	: meter(config, queue) {
	// find the file name for the datatypes
	std::string	filename = config.stringvalue("datafields");
	debug(LOG_DEBUG, DEBUG_LOG, 0, "field configuration: %s",
		filename.c_str());
	parsefields(filename);

	// get the host name of the meter
	std::string	hostname = config.stringvalue("meterhostname",
		"localhost");

	// get the port number from the configuration
	int	port = config.intvalue("meterport", 502);
	debug(LOG_DEBUG, DEBUG_LOG, 0, "using port %d", port);

	// connect
	connect(hostname, port);

	// start the thread
	startthread();
}

/**
 * \brief Destroy the modbus device
 */
modbus_meter::~modbus_meter() {
	modbus_close(mb);
	modbus_free(mb);
}

const std::list<modbus_meter::modrec_t>::const_iterator	modbus_meter::byname(const std::string& name) {
	return	std::find_if(datatypes.begin(), datatypes.end(),
		[name](const modrec_t m) -> bool {
			return (m.name == name);
		}
	);
}

float	modbus_meter::get(const modrec_t modrec) {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "getting %s", modrec.name.c_str());
	if (modrec.type == m_phases) {
		return get_phases(modrec);
	}
	modbus_set_slave(mb, modrec.unit);
	unsigned short	u;
	if (modbus_read_registers(mb, modrec.address, 1, &u) < 0) {
		debug(LOG_ERR, DEBUG_LOG, 0, "read failure, reconnecting");
		reconnect();
		if (modbus_read_registers(mb, modrec.address, 1, &u) < 0) {
			debug(LOG_ERR, DEBUG_LOG, 0, "failure after reconnect");
			throw std::runtime_error("failure to reconnect");
		}
	}
	float	value = 0.;
	if (m_uint16 == modrec.type) {
		value = u * modrec.scalefactor;
	}
	if (m_int16 == modrec.type) {
		value = ((short)u) * modrec.scalefactor;
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "%s -> %.1f",
		modrec.name.c_str(), value);
	return value;
}

float	modbus_meter::get_phases(const modrec_t modrec) {
	float	phase1 = get(*byname(modrec.name + "_phase1"));
	float	phase2 = get(*byname(modrec.name + "_phase2"));
	float	phase3 = get(*byname(modrec.name + "_phase3"));
	debug(LOG_DEBUG, DEBUG_LOG, 0, "sum of three phases: "
		"%.0f + %.0f + %.0f", phase1, phase2, phase3);
	return phase1 + phase2 + phase3;
}

message	modbus_meter::integrate() {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "integrate a message");
	std::unique_lock<std::mutex>    lock(_mutex);

	// compute the time for the next minute interval
	std::chrono::system_clock::time_point   start
		= std::chrono::system_clock::now();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start is %ld",
		start.time_since_epoch().count());

	debug(LOG_DEBUG, DEBUG_LOG, 0, "seconds: %d",
		start.time_since_epoch()
			* std::chrono::system_clock::period::num
			/ std::chrono::system_clock::period::den);

	// round down to the start of the minute
	std::chrono::seconds    startduration
		= std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::duration_cast<std::chrono::minutes>(
			start.time_since_epoch()));
	debug(LOG_DEBUG, DEBUG_LOG, 0, "startduration is %ld",
		startduration.count());
	start = std::chrono::time_point<std::chrono::system_clock,
		std::chrono::seconds>(startduration);

	auto    end = start + std::chrono::seconds(60);
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start: %ld, end: %ld",
		start.time_since_epoch().count(),
		end.time_since_epoch().count());

	// create the result
	message result(start);
	std::chrono::system_clock::time_point   previous = start;

	// ensure that pos/neg fields are always present
	for (auto i = datatypes.begin(); i != datatypes.end(); i++) {
		if (i->op == m_signed) {
			result.accumulate(std::chrono::seconds(0),
				i->name + "_pos", 0.);
			result.accumulate(std::chrono::seconds(0),
				i->name + "_neg", 0.);
		}
	}

	// iterate until the end
	std::chrono::system_clock::time_point   now;
	int     counter = 0;
	while ((now = std::chrono::system_clock::now()) < end) {
		// compute the remaining time
		std::chrono::duration<float>    remaining = end - now;
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "remaining: %.3f",
		//      remaining.count());

		// compute the largest possible interval we can wait
		if (remaining > _interval) {
			remaining = _interval;
		}

		// wait for the remaining time
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "waiting for %.3f",
		//      remaining.count());
		switch (_signal.wait_for(lock, remaining)) {
		case std::cv_status::timeout:
			//debug(LOG_DEBUG, DEBUG_LOG, 0, "timeout");
			break;
		case std::cv_status::no_timeout:
			// this means we were signaled to interrup
			//debug(LOG_DEBUG, DEBUG_LOG, 0, "wait interrupted");
			std::string     msg("meter thread interrupted by signal");
		debug(LOG_DEBUG, DEBUG_LOG, 0, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}

		// end time for this integration step
		std::chrono::duration<float>    delta(
			std::chrono::system_clock::now() - previous);
		debug(LOG_DEBUG, DEBUG_LOG, 0, "delta: %.3f", delta.count());
		previous = std::chrono::system_clock::now();

		// read the data
		for (auto i = datatypes.begin(); i != datatypes.end(); i++) {
			float	value = get(*i);
			if (i->op == m_average) {
				result.accumulate(delta, i->name, value);
			}
			if (i->op == m_max) {
				result.update(i->name, value);
			}
			if (i->op == m_signed) {
				result.accumulate_signed(delta, i->name, value);
			}
		}

		//debug(LOG_DEBUG, DEBUG_LOG, 0, "processing a packet");
		counter++;
	}

	// when we get here, we are at the end of the interval, so we now
	// have to extrapolate to the full minute
	float   d = std::chrono::duration<double>(end - start).count();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "duration was %.6f", d);

	// finalize the message
	float   factor = 1. / d;
	for (auto i = datatypes.begin(); i != datatypes.end(); i++) {
		switch (i->op) {
		case m_average:
			result.finalize(i->name, factor);
			break;
		case m_signed:
			result.finalize(i->name + "_pos", factor);
			result.finalize(i->name + "_neg", factor);
			break;
		default:
			break;
		}
	}

	// return the message
	return result;
}

} // namespace powermeter
