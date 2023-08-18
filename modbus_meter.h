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
		m_uint16, m_int16, m_phases
	} datatype_t;
	typedef enum {
		m_average, m_max, m_min, m_signed
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
	std::string	_hostname;
	int	_port;
	void	connect(const std::string& hostname, int port);
	void	reconnect();
	void	connect_common();
private:
	modbus_t	*mb;
	std::list<modrec_t>	datatypes;
	void	parsefields(const std::string& filename);
	float	get(const modrec_t modrec);
	float	get_phases(const modrec_t modrec);
	const std::list<modrec_t>::const_iterator	byname(const std::string& name);
protected:
	virtual message integrate();
public:
	modbus_meter(const configuration& config, messagequeue& queue);
	virtual ~modbus_meter();
};

} // namespace powermeter

#endif /* _modbus_meter_h */
