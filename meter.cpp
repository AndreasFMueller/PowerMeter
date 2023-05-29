/*
 * meter.cpp
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <meter.h>
#include <stdexcept>
#include <debug.h>
#include <format.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace powermeter {

/**
 * \brief Static method to launch the meter thread
 *
 * \param m	the meter class
 */
void	meter::launch(meter *m) {
	debug(LOG_DEBUG, DEBUG_LOG, 0, "launching the meter thread");
	try {
		m->run();
	} catch (const std::exception& x) {
		std::string	msg = stringprintf("meter thread terminated "
			"by exception: %s", x.what());
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		return;
	} catch (...) {
		debug(LOG_ERR, DEBUG_LOG, 0, "meter thread terminated by "
			"unknown exception");
		return;
	}
	debug(LOG_ERR, DEBUG_LOG, 0, "meter thread ends");
}

void	meter::startthread() {
	// run the thread
	debug(LOG_DEBUG, DEBUG_LOG, 0, "launching the meter thread");
	std::unique_lock<std::mutex>	lock(_mutex);
	_active = true;
	_thread = std::thread(launch, this);
}

void	meter::stopthread() {
	{
		debug(LOG_DEBUG, DEBUG_LOG, 0, "notify the meter thread");
		std::unique_lock<std::mutex>	lock(_mutex);
		_active = false;
		_signal.notify_all();
	}
	if (_thread.joinable()) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "wait for meter thread to "
			"complete");
		_thread.join();
		debug(LOG_DEBUG, DEBUG_LOG, 0, "meter thread ended");
	}
}

/**
 * \brief Constructor for a meter object
 *
 * \param hostname	the name of the meter host
 * \param port		the port of the modbustcp implementation
 * \param deviceid	the device id of the modbustcp device
 * \param queue		the queue to place messages on
 */
meter::meter(const configuration& config, messagequeue& queue)
	: _queue(queue),
	  _interval(std::chrono::duration<float>(
		config.floatvalue("meterinterval"))) {
}

/**
 * \brief Destroy the meter class
 */
meter::~meter() {
}

/**
 * \brief The main method for the meter thread
 */
void	meter::run() {
	while (_active) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "wait for a message");
		// during an integration interval
		try {
			message	m = integrate();
			debug(LOG_DEBUG, DEBUG_LOG, 0, "got a new message");
			
			// submit it to the queue
			debug(LOG_DEBUG, DEBUG_LOG, 0, "submit message");
			_queue.submit(m);
		} catch (const std::exception& x) {
			debug(LOG_ERR, DEBUG_LOG, 0, "cannot process a "
				"message: %s, %s", x.what(),
				(_active) ? "retry" : "terminate");
		}
	}
	debug(LOG_INFO, DEBUG_LOG, 0, "meter thread has been deactivated");
}

} // namespace powermeter
