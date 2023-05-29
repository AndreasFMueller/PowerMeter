//
// meterfactory.cpp
//
// (c) 2024 Prof Dr Andreas Müller
//
#include <meterfactory.h>
#include <solivia_meter.h>
#include <ale3_meter.h>
#include <modbus_meter.h>
#include <format.h>
#include <debug.h>

namespace powermeter {

std::shared_ptr<meter>	meterfactory::get(const std::string& metertypename,
		messagequeue& queue) {
	if (metertypename == "solivia") {
		return std::shared_ptr<meter>(new solivia_meter(_config, queue));
	}
	if (metertypename == "ale3") {
		return std::shared_ptr<meter>(new ale3_meter(_config, queue));
	}
	if (metertypename == "modbus") {
		return std::shared_ptr<meter>(new modbus_meter(_config, queue));
	}
	std::string	msg = stringprintf("unknown meter type: %s",
		metertypename.c_str());
	debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
	throw std::runtime_error(msg);
}

} // namespace powermeter
