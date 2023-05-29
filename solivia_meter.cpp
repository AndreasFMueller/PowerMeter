//
// solivia_meter.cpp
//
// (c) 2023 Prof Dr Andreas Müller
//

#include <solivia_meter.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <debug.h>
#include <format.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <boost/crc.hpp>

namespace powermeter {

/**
 * \brief Solivia meter constructor
 *
 * \param config	configuration to get parameters from
 * \param queue		the queue to send the data to
 */
solivia_meter::solivia_meter(const configuration& config, messagequeue& queue)
	: meter(config, queue),
	  _receive_port(config.intvalue("listenport")),
	  _send_port(config.intvalue("meterport")),
	  _id(config.intvalue("meterid")),
	  _passive(config.boolvalue("meterpassive")),
	  _request { 0x02, 0x05, _id, 0x02, 0x60, 0x01, 0x85, 0xfc, 0x03 } {
	// create the listen port
	_receive_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (_receive_fd < 0) {
		std::string	msg = stringprintf("cannot create socket: %s",
			strerror(errno));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// bind
	struct sockaddr_in	sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(_receive_port);
	sa.sin_addr.s_addr = INADDR_ANY;
	if (bind(_receive_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)  {
		std::string	msg = stringprintf("cannot bind: %s",
			strerror(errno));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	debug(LOG_DEBUG, DEBUG_LOG, 0, "listen socket initialized");

	// create the send socket
	_send_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (_send_fd < 0) {
		std::string	msg = stringprintf("cannot create socket: %s",
			strerror(errno));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// get the host name
	std::string	hostname = config.stringvalue("meterhostname");
	debug(LOG_DEBUG, DEBUG_LOG, 0, "meter hostname: %s",
		hostname.c_str());
	struct hostent	*hp = gethostbyname(hostname.c_str());
	if (NULL == hp) {
		std::string	msg = stringprintf("cannot resolve '%s': %s",
			hostname.c_str(), strerror(errno));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "found ip address: %s (%d)",
		inet_ntoa(*(in_addr*)hp->h_addr), hp->h_length);

	// create the socket address 
	_addr.sin_family = AF_INET;
	_addr.sin_port = htons(_send_port);
	memcpy(&(_addr.sin_addr), hp->h_addr, hp->h_length);
	debug(LOG_DEBUG, DEBUG_LOG, 0, "copied %d address bytes, %s:%hd",
		hp->h_length, inet_ntoa(_addr.sin_addr), ntohs(_addr.sin_port));

	// compute the solivia checksum
	debug(LOG_DEBUG, DEBUG_LOG, 0, "compute the request CRC");
	boost::crc_16_type	crc;
	crc.process_bytes(_request + 1, 5);
	unsigned short	c = crc.checksum();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "crc: %04x", c);
	_request[6] = (c & 0xff);
	_request[7] = (c >> 8) & 0xff;
	std::string	p;
	for (unsigned int i = 0; i < sizeof(_request); i++) {
		p = p + stringprintf(" %02x", _request[i]);
	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "request packet: %s", p.c_str());

	// start the thread
	startthread();
}

/**
 * \brief Destructor for the solivia meter class
 */
solivia_meter::~solivia_meter() {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "closing the socket");
	stopthread();
	close(_receive_fd);
	_receive_fd = -1;
}

/**
 * \brief Get an unsigned short from a certain offset in the data packet
 *
 * \param offset	offset
 */
unsigned short solivia_meter::shortat(unsigned int offset) const {
	unsigned short	result = _packet[offset];
	result <<= 8;
	result += _packet[offset + 1];
	return result;
}

/**
 * \brief Get a float from the packet at a certain offest
 *
 * \param offset	the offset where to get the number
 * \param scale		factor to rescale the number
 */
float	solivia_meter::floatat(unsigned int offset, float scale) const {
	return scale * shortat(offset);
}

/**
 * \brief Extract a string of a given length from the packet
 *
 * \param offset	offset of the string in the packet
 * \param length	the length of the string
 */
std::string	solivia_meter::stringat(unsigned int offset, size_t length) const {
	return std::string((const char *)(_packet + offset), length);
}

/**
 * \brief Extract a version string from two bytes at a given offset
 *
 * \param offset	the offset of the version number
 */
std::string	solivia_meter::versionat(unsigned int offset) const {
	return stringprintf("%d.%d", _packet[offset], _packet[offset + 1]);
}

/**
 * \brief Retrieve a float from 4 bytes at a given offset
 *
 * \param offset	the offset of the number
 * \param scale		the scaling factor
 */
float	solivia_meter::longfloatat(unsigned int offset, float scale) const {
	unsigned long	result = _packet[offset];
	for (int i = 1; i <= 3; i++) {
		result <<= 8;
		result += _packet[offset + i];
	}
	return scale * result;
}

/**
 * \brief Retrieve a packet
 */
int	solivia_meter::getpacket() {
	//debug(LOG_DEBUG, DEBUG_LOG, 0, "get a packet");
	// send a packet
	int	rc;
	if (_passive) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "passive mode");
	} else {
		rc = sendto(_send_fd, _request, sizeof(_request), 0,
				(struct sockaddr *)&_addr, sizeof(_addr));
		if (rc < 0) {
			std::string	msg = stringprintf("cannot send "
				"request: %s", strerror(errno));
			debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
	}

	// wait for at most a second for packets and check whether they
	// are useful
	fd_set	fds;
	FD_ZERO(&fds);
	FD_SET(_receive_fd, &fds);
	struct timeval	tv = { .tv_sec = 1, .tv_usec = 0 };
	while (1 == (rc = select(_receive_fd + 1, &fds, NULL, NULL, &tv))) {
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "time: %d.%06d", tv.tv_sec,
		//	tv.tv_usec);
		// read the packet
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "reading a packet");
		rc = read(_receive_fd, _packet, packetsize);
		if (rc < 0) {
			debug(LOG_ERR, DEBUG_LOG, 0, "cannot read packet: %s",
				strerror(errno));
			continue;
		}

		// check packet size
		if (rc != packetsize) {
			debug(LOG_DEBUG, DEBUG_LOG, 0,
				"wrong packet size (%d), skipping", rc);
			continue;
		}

		// skip if this is a bad packet
		if ((0x02 != stx()) || (0x06 != ack())) {
			debug(LOG_ERR, DEBUG_LOG, 0, "incorrect packet "
				"format, skipping");
			continue;
		}

		// check the id
		if (_id != id()) {
			debug(LOG_ERR, DEBUG_LOG, 0, "ID mismatch, skipping");
			continue;
		}

		// check the CRC
		boost::crc_16_type	crc;
		crc.process_bytes(_packet + 1, packetsize - 4);
		if (crc.checksum() != crc()) {
			debug(LOG_ERR, DEBUG_LOG, 0,
				"bad backed CRC: %hu != %hu, ignoring",
				crc.checksum(), crc());
			continue;
		}

		// if we get to this point, then we have a correct packet
		// in the packet buffer
		return 1;
	}
	// handle the case where select did not work
	if (rc < 0) {
		std::string	msg = stringprintf("select error: %s",
			strerror(errno));
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	// handle timeout
	if (rc == 0) {
		debug(LOG_ERR, DEBUG_LOG, 0, "no packet, timeout");
		return 0;
	}
	// should not get here 
	return 1; // packet retrieved
}

/**
 * \brief Integrate packets until the end of the next minute
 */
message	solivia_meter::integrate() {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "integrate a message");
	std::unique_lock<std::mutex>	lock(_mutex);

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

		// wait for the remaining time
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "waiting for %.3f",
		//	remaining.count());
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
		lock.unlock();
		if (0 == getpacket()) {
			debug(LOG_ERR, DEBUG_LOG, 0, "no packet, maybe lost, "
				"trying next packet");
			continue;
		}
		lock.lock();

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

