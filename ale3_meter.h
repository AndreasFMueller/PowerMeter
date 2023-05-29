/*
 * ale3_meter.h -- connection to the power meter
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#ifndef _ale3_meter_h
#define _ale3_meter_h

#include <meter.h>
#include <message.h>
#include <modbus.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <configuration.h>
#include <simulator.h>

namespace powermeter {

class ale3_meter : public meter {
protected:
	// configuration data
	std::string		_hostname;
	unsigned short		_port;
	int			_deviceid;

	// the connection
	modbus_t		*_mb;

	// reimplement the integration method
	virtual message	integrate();
public:
	ale3_meter(const configuration& config, messagequeue& queue);
	~ale3_meter();
private:
	simulator	sim;
	void	read(unsigned short *registers);
public:
	static bool	simulate;
};

} // namespace powermeter

#endif /* _ale3_meter_h */
