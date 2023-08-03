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
			// store the record
			datatypes.push_back(record);
			debug(LOG_DEBUG, DEBUG_LOG, 0, "added type '%s'",
				record.name.c_str());
		}
		in.getline(buffer, sizeof(buffer));
	}
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

	// get the ip address of the modbus device
	struct hostent	*hp = gethostbyname(hostname.c_str());
	if (NULL == hp) {
		std::string	msg = stringprintf("cannot resolve '%s'",
			hostname.c_str());
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg);
		throw std::runtime_error(msg);
	}

	// check that we have an address
	if (NULL == hp->h_addr) {
		std::string	msg = stringprintf("no address for '%s'",
			hostname);
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	
	// convert IP address to string
	struct in_addr	*ia = (struct in_addr *) hp->h_addr_list[0];
	char	*ip = strdup(inet_ntoa(*ia));
	debug(LOG_DEBUG, DEBUG_LOG, 0, "connecting to IP %s", ip);

	// get the port number from the configuration
	int	port = config.intvalue("meterport", 502);
	debug(LOG_DEBUG, DEBUG_LOG, 0, "using port %d", port);

	// initialize the modbus device
	mb = modbus_new_tcp(ip, port);
	if (NULL == mb) {
		free(ip);
		std::string	msg = stringprintf("cannot create");
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
		hostname.c_str(), port);

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

#if 0
		// get a new packet
		// XXX get a packet
		unsigned short	data1[12];
		modbus_set_slave(mb, 100);
		modbus_read_registers(mb, 808, 12, data1);
		debug(LOG_DEBUG, DEBUG_LOG, 0, "PV output: %hu, %hu, %hu",
			data1[0], data1[1], data1[2]);
		debug(LOG_DEBUG, DEBUG_LOG, 0, "PV input: %hu, %hu, %hu",
			data1[3], data1[4], data1[5]);
		debug(LOG_DEBUG, DEBUG_LOG, 0, "consumption: %hu, %hu, %hu",
			data1[9], data1[10], data1[11]);

		short	grid[3];
		modbus_read_registers(mb, 820, 3, (unsigned short*)grid);
		debug(LOG_DEBUG, DEBUG_LOG, 0, "grid: %hd, %hd, %hd",
			grid[0], grid[1], grid[2]);

		unsigned short	battery[7];
		modbus_read_registers(mb, 840, 7, battery);
		debug(LOG_DEBUG, DEBUG_LOG, 0, "battery: voltage %.1f, current %.1f, power %hd, charge %hu, state %hu, ah %.1f, time to go %.0f",
			battery[0] / 10.,
			((short)battery[1]) / 10.,
			(short)battery[2],
			battery[3],
			battery[4],
			- battery[5] / 10.,
			battery[6] * 100.);
#endif

		// end time for this integration step
		std::chrono::duration<float>    delta(
			std::chrono::system_clock::now() - previous);
		debug(LOG_DEBUG, DEBUG_LOG, 0, "delta: %.3f", delta.count());
		previous = std::chrono::system_clock::now();

		// read the data
		for (auto i = datatypes.begin(); i != datatypes.end(); i++) {
			unsigned short	u;
			modbus_set_slave(mb, i->unit);
			modbus_read_registers(mb, i->address, 1, &u);
			float	value = 0.;
			if (m_uint16 == i->type) {
				value = u * i->scalefactor;
			}
			if (m_int16 == i->type) {
				value = ((short)u) * i->scalefactor;
			}
			debug(LOG_DEBUG, DEBUG_LOG, 0, "%s -> %.1f",
				i->name.c_str(), value);
			if (i->op == m_average) {
				result.accumulate(delta, i->name, value);
			}
			if (i->op == m_max) {
				result.update(i->name, value);
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
	result.finalize("pv.prms_phase1", factor);
	result.finalize("pv.prms_phase2", factor);
	result.finalize("pv.prms_phase3", factor);
	result.finalize("consumption.prms_phase1", factor);
	result.finalize("consumption.prms_phase2", factor);
	result.finalize("consumption.prms_phase3", factor);
	result.finalize("grid.prms_phase1", factor);
	result.finalize("grid.prms_phase2", factor);
	result.finalize("grid.prms_phase3", factor);
	result.finalize("battery.power", factor);
	result.finalize("battery.charge", factor);

	// return the message
	return result;
}

} // namespace powermeter
