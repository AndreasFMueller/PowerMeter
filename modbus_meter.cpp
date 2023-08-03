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

namespace powermeter {

/**
 * \brief Construct a modbus power meter
 *
 * \param config	the configuration to use to get parameters
 * \param queue		the message queue to use to send messages
 */
modbus_meter::modbus_meter(const configuration& config, messagequeue& queue)
	: meter(config, queue) {
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

		// get a new packet
		// XXX get a packet

		// end time for this integration step
		std::chrono::duration<float>    delta(std::chrono::system_clock::now() - previous);
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "delta: %.3f", delta.count());
		previous = std::chrono::system_clock::now();

		//debug(LOG_DEBUG, DEBUG_LOG, 0, "processing a packet");
		counter++;
	}

	// when we get here, we are at the end of the interval, so we now
	// have to extrapolate to the full minute
	float   d = std::chrono::duration<double>(end - start).count();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "duration was %.6f", d);

	// finalize the message


	// return the message
	return result;
}

} // namespace powermeter
