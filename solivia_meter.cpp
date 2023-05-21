//
// solivia_meter.cpp
//
// (c) 2023 Prof Dr Andreas Müller
//

#include <solivia_meter.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <debug.h>
#include <format.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>

namespace powermeter {

/**
 * \brief Solivia meter constructor
 */
solivia_meter::solivia_meter(const configuration& config, messagequeue& queue)
	: meter(config, queue),
	  _port(config.intvalue("listenport")) {
	// create the listen port
	_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (_fd < 0) {
		std::string	msg = stringprintf("cannot create socket: %s",
			strerror(errno));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// bind
	struct sockaddr_in	sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(_port);
	sa.sin_addr.s_addr = INADDR_ANY;
	if (bind(_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)  {
		std::string	msg = stringprintf("cannot bind: %s",
			strerror(errno));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	debug(LOG_DEBUG, DEBUG_LOG, 0, "socket initialized");

	// start the thread
	startthread();
}


solivia_meter::~solivia_meter() {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "closing the socket");
	stopthread();
	close(_fd);
	_fd = -1;
}

unsigned short solivia_meter::shortat(unsigned int offset) const {
	unsigned short	result = _packet[offset];
	result <<= 8;
	result += _packet[offset + 1];
	return result;
}

float	solivia_meter::floatat(unsigned int offset, float scale) const {
	return scale * shortat(offset);
}

std::string	solivia_meter::stringat(unsigned int offset, size_t length) const {
	return std::string((const char *)(_packet + offset), length);
}

std::string	solivia_meter::versionat(unsigned int offset) const {
	return stringprintf("%d.%d", _packet[offset], _packet[offset + 1]);
}

float	solivia_meter::longfloatat(unsigned int offset, float scale) const {
	unsigned long	result = _packet[offset];
	for (int i = 1; i <= 3; i++) {
		result <<= 8;
		result += _packet[offset + i];
	}
	return scale * result;
}

message	solivia_meter::integrate() {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "integrate a message");
	std::unique_lock<std::mutex>	lock(_mutex);

	// compute the time for the next minute interval
	std::chrono::system_clock::time_point   start
		= std::chrono::system_clock::now();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start is %ld", start.time_since_epoch().count());

	debug(LOG_DEBUG, DEBUG_LOG, 0, "seconds: %d", start.time_since_epoch() * std::chrono::system_clock::period::num / std::chrono::system_clock::period::den);

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
	int	counter = 0;
	while ((now = std::chrono::system_clock::now()) < end) {
		// compute the remaining time
		std::chrono::duration<float>    remaining = end - now;
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "remaining: %.3f",
		//	remaining.count());

		// compute the largest possible interval we can wait
		if (remaining > _interval) {
			remaining = _interval;
		}

		// wait for the remaining time (use select for this, but need
		// to unlock the lock while waiting)
		fd_set	fds;
		FD_ZERO(&fds);
		FD_SET(_fd, &fds);
		struct timeval	tv;
		tv.tv_sec = floor(remaining.count());
		tv.tv_usec = 1000000 * (remaining.count() - tv.tv_sec);
		lock.unlock();
		int	rc = select(_fd + 1, &fds, NULL, NULL, &tv);
		lock.lock();
		if (rc < 0) {
			std::string	msg = stringprintf("select error: %s",
				strerror(errno));
			debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
		if (rc == 0) {
			debug(LOG_DEBUG, DEBUG_LOG, 0, "no packet");
			// stop processing, retrying it will end the loop
			continue;
		}

		// read the packet
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "reading a packet");
		rc = read(_fd, _packet, packetsize);
		if (rc < 0) {
			std::string	 msg = stringprintf("cannot read: %s",
				strerror(errno));
			debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
		if (rc != packetsize) {
			//debug(LOG_DEBUG, DEBUG_LOG, 0,
			//	"wrong packet size (%d), skipping", rc);
			continue;
		}

		// skip if this is a bad packet
		if ((0x02 != stx()) || (0x06 != ack())) {
			debug(LOG_DEBUG, DEBUG_LOG, 0, "strange packet");
			continue;
		}

		// end time for this integration step
		std::chrono::duration<float>    delta(std::chrono::system_clock::now() - previous);
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "delta: %.3f", delta.count());
		previous = std::chrono::system_clock::now();

		//debug(LOG_DEBUG, DEBUG_LOG, 0, "processing a packet");
		counter++;

		// accumulate the data
		result.accumulate(delta, "phase1.voltage", phase1_voltage());
		result.accumulate(delta, "phase1.current", phase1_current());
		result.accumulate(delta, "phase1.power", phase1_power());
		result.accumulate(delta, "phase1.frequency", phase1_frequency());

		result.accumulate(delta, "phase2.voltage", phase2_voltage());
		result.accumulate(delta, "phase2.current", phase2_current());
		result.accumulate(delta, "phase2.power", phase2_power());
		result.accumulate(delta, "phase2.frequency", phase2_frequency());

		result.accumulate(delta, "phase3.voltage", phase3_voltage());
		result.accumulate(delta, "phase3.current", phase3_current());
		result.accumulate(delta, "phase3.power", phase3_power());
		result.accumulate(delta, "phase3.frequency", phase3_frequency());

		result.accumulate(delta, "string1.voltage", string1_voltage());
		result.accumulate(delta, "string1.current", string1_current());
		result.accumulate(delta, "string1.power", string1_power());

		result.accumulate(delta, "string2.voltage", string2_voltage());
		result.accumulate(delta, "string2.current", string2_current());
		result.accumulate(delta, "string2.power", string2_power());

		result.accumulate(delta, "inverter.power", power());
		result.update("inverter.feedtime", feedtime());
		result.update("inverter.energy", energy());
		result.accumulate(delta, "inverter.temperature", temperature());
	}

        // when we get here, we are at the end of the interval, so we now
        // have to extrapolate to the full minute
        float   d = std::chrono::duration<double>(end - start).count();
        debug(LOG_DEBUG, DEBUG_LOG, 0, "duration was %.6f", d);

	float	factor = 1. / d;
	result.finalize("phase1.voltage", factor);
	result.finalize("phase1.current", factor);
	result.finalize("phase1.power", factor);
	result.finalize("phase1.frequency", factor);

	result.finalize("phase2.voltage", factor);
	result.finalize("phase2.current", factor);
	result.finalize("phase2.power", factor);
	result.finalize("phase2.frequency", factor);

	result.finalize("phase3.voltage", factor);
	result.finalize("phase3.current", factor);
	result.finalize("phase3.power", factor);
	result.finalize("phase3.frequency", factor);

	result.finalize("string1.voltage", factor);
	result.finalize("string1.current", factor);
	result.finalize("string1.power", factor);

	result.finalize("string2.voltage", factor);
	result.finalize("string2.current", factor);
	result.finalize("string2.power", factor);

	result.finalize("inverter.power", factor);
	result.finalize("inverter.temperature", factor);

	debug(LOG_DEBUG, DEBUG_LOG, 0, "message finalized with %d packets",
		counter);

	// return the message
	return result;
}

} // namespace powermeter

