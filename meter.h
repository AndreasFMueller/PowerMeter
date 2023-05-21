/*
 * meter.h -- connection to the power meter
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#ifndef _meter_h
#define _meter_h

#include <message.h>
#include <modbus.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <configuration.h>
#include <simulator.h>

namespace powermeter {

class meter;

class meter {
protected:
	messagequeue&		_queue;
	std::chrono::duration<float>	_interval;
	// managing the thread
	std::atomic<bool>	_active;
	std::thread		_thread;
	std::mutex		_mutex;
	std::condition_variable	_signal;
	virtual message	integrate() = 0;

	void	stopthread();
public:
	meter(const configuration& config, messagequeue& queue);
	meter(const meter& other) = delete;
	virtual ~meter();
	void	startthread();
	static void	launch(meter* m);
	void	run();
};

} // namespace powermeter

#endif /* _meter_h */
