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

namespace powermeter {

class meter;

class meter {
	// configuration data
	std::string		_hostname;
	unsigned short		_port;
	unsigned char		_deviceid;
	messagequeue&		_queue;
	// the connection
	modbus_t		*_mb;
	// managing the thread
	std::atomic<bool>	_active;
	std::thread		_thread;
	std::mutex		_mutex;
	std::condition_variable	_signal;
	message	integrate();
public:
	meter(const std::string& hostname, unsigned short port,
		unsigned char deviceid, messagequeue& queue);
	~meter();
	static void	launch(meter* m);
	void	run();
};

} // namespace powermeter

#endif /* _meter_h */
