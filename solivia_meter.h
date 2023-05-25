//
// solivia_meter.h
//
// (c) 2023 Prof Dr Andreas Müller
//
#ifndef _solivia_meter_h
#define _solivia_meter_h

#include <meter.h>
#include <netinet/in.h>

namespace powermeter {

class solivia_meter : public meter {
	short	_receive_port;
	int	_receive_fd;
	struct sockaddr_in	_addr;
	short	_send_port;
	int	_send_fd;
	unsigned char	_id;
	bool	_passive;
	unsigned char	_request[9];
	// analysis of a packet
	static const size_t	packetsize = 164;
	unsigned char	_packet[packetsize];
	// access functions
	unsigned short	shortat(unsigned int offset) const;
	float	floatat(unsigned int offset, float scale) const;
	float	longfloatat(unsigned int offset, float scale) const;
	std::string	stringat(unsigned int offset, size_t length) const;
	std::string	versionat(unsigned int offset) const;
	// packet structure
	unsigned char	stx() const { return _packet[0]; }
	unsigned char	ack() const { return _packet[1]; }
	unsigned char	id() const { return _packet[2]; }
	size_t	length() const { return _packet[3]; }
	unsigned short	cmd() const { return shortat(4); }
	static const size_t	partoffset = 6;
	std::string	part() const { return stringat(partoffset, 11); }
	static const size_t	serialoffset = partoffset + 11;
	std::string	serial() const { return stringat(serialoffset, 18); }
	static const size_t	version = serialoffset + 24;
	std::string	pm_firmware() const { return versionat(version); }
	std::string	sts_firmware() const { return versionat(version + 4); }
	std::string	dsp_firmware() const { return versionat(version + 8); }
	static const size_t	phase1 = version + 12;
	float	phase1_voltage() const { return floatat(phase1, 0.1); }
	float	phase1_current() const { return floatat(phase1 + 2, 0.01); }
	float	phase1_power() const { return floatat(phase1 + 4, 1); }
	float	phase1_frequency() const { return floatat(phase1 + 6, 0.01); }
	static const size_t	phase2 = phase1 + 12;
	float	phase2_voltage() const { return floatat(phase2, 0.1); }
	float	phase2_current() const { return floatat(phase2 + 2, 0.01); }
	float	phase2_power() const { return floatat(phase2 + 4, 1); }
	float	phase2_frequency() const { return floatat(phase2 + 6, 0.01); }
	static const size_t	phase3 = phase2 + 12;
	float	phase3_voltage() const { return floatat(phase3, 0.1); }
	float	phase3_current() const { return floatat(phase3 + 2, 0.01); }
	float	phase3_power() const { return floatat(phase3 + 4, 1); }
	float	phase3_frequency() const { return floatat(phase3 + 6, 0.01); }
	static const size_t	string1 = phase3 + 12;
	float	string1_voltage() const { return floatat(string1, 0.1); }
	float	string1_current() const { return floatat(string1 + 2, 0.01); }
	float	string1_power() const { return floatat(string1 + 4, 1); }
	static const size_t	string2 = string1 + 6;
	float	string2_voltage() const { return floatat(string2, 0.1); }
	float	string2_current() const { return floatat(string2 + 2, 0.01); }
	float	string2_power() const { return floatat(string2 + 4, 1); }
	static const size_t	inverter = string2 + 6;
	float	power() const { return floatat(inverter, 1); }
	float	energy() const { return longfloatat(inverter + 6, 1); }
	float	feedtime() const { return longfloatat(inverter + 10, 1); }
	float	totalenergy() const { return longfloatat(inverter + 14, 1); }
	float	temperature() const { return floatat(inverter + 22, 1); }
	unsigned short	crc() const { return shortat(packetsize - 3); }
	unsigned char	etx() const { return _packet[packetsize - 1]; }
	int	getpacket();
protected:
	virtual message	integrate();
public:
	solivia_meter(const configuration& config, messagequeue& queue);
	~solivia_meter();
};

} // namespace powermeter

#endif /* _solivia_meter_h */
