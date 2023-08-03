/*
 * modbus_meter.h -- generic modbus based meter
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#ifndef _modbus_meter_h
#define _modbus_meter_h

#include <meter.h>
#include <modbus.h>
#include <list>

namespace powermeter {

class modbus_meter : public meter {
public:
	typedef enum {
		m_uint16, m_int16
	} datatype_t;
	typedef enum {
		m_average, m_max, m_min
	} operator_t;
	typedef struct {
		//meteoname,unit,address,type,scalefactor,operator
		std::string	name;
		unsigned short	unit;
		unsigned short	address;
		datatype_t	type;
		float		scalefactor;
		operator_t	op;
	}	modrec_t;
private:
	modbus_t	*mb;
	std::list<modrec_t>	datatypes;
	void	parsefields(const std::string& filename);
protected:
	virtual message integrate();
public:
	modbus_meter(const configuration& config, messagequeue& queue);
	virtual ~modbus_meter();
};

} // namespace powermeter

#endif /* _modbus_meter_h */
