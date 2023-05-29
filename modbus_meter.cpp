/*
 * modbus_meter.cpp
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <modbus_meter.h>
#include <debug.h>

namespace powermeter {

modbus_meter::modbus_meter(const configuration& config, messagequeue& queue)
	: meter(config, queue) {
}

modbus_meter::~modbus_meter() {
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
		// XXX get a package

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
