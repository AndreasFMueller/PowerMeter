/*
 * modbus_meter.h -- generic modbus based meter
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#ifndef _modbus_meter_h
#define _modbus_meter_h

#include <meter.h>
#include <modbus.h>

namespace powermeter {

class modbus_meter : public meter {
	modbus_t	*mb;
protected:
	virtual message integrate();
public:
	modbus_meter(const configuration& config, messagequeue& queue);
	virtual ~modbus_meter();
};

} // namespace powermeter

#endif /* _modbus_meter_h */
