/*
 * modbus_meter.cpp
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <modbus_meter.h>
#include <stdexcept>
#include <debug.h>
#include <format.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace powermeter {

bool	modbus_meter::simulate = false;

/**
 * \brief Constructor for a meter object
 *
 * \param config	configuration
 * \param queue		the queue to place messages on
 */
modbus_meter::modbus_meter(const configuration& config, messagequeue& queue)
	: meter(config, queue),
	  _hostname(config.stringvalue("meterhostname")),
	  _port(config.intvalue("meterport")),
	  _deviceid(config.intvalue("meterid")) {

	// set up the connection
	_mb = NULL;
	if (simulate) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "using simulated meter");
	} else {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "create context to %s:%d",
			_hostname.c_str(), _port);
		_mb = modbus_new_tcp(_hostname.c_str(), _port);
		if (NULL == _mb) {
			std::string	msg = stringprintf("cannot create "
				"modbus context to %s:%p: %s",
				_hostname.c_str(), _port,
				modbus_strerror(errno));
			debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
		if (0 != modbus_set_response_timeout(_mb, 0, 2000)) {
			std::string	msg = stringprintf("cannot set timeout:"
				" %s", modbus_strerror(errno));
			debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
			modbus_close(_mb);
			modbus_free(_mb);
			_mb = NULL;
			throw std::runtime_error(msg);
		}
		debug(LOG_DEBUG, DEBUG_LOG, 0, "connecting");
		if (-1 == modbus_connect(_mb)) {
			std::string	msg = stringprintf("cannot connect: %s",
				modbus_strerror(errno));
			debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
			modbus_close(_mb);
			modbus_free(_mb);
			_mb = NULL;
			throw std::runtime_error(msg);
		}
		debug(LOG_DEBUG, DEBUG_LOG, 0, "set slave to %d", _deviceid);
		if (-1 == modbus_set_slave(_mb, _deviceid)) {
			debug(LOG_ERR, DEBUG_LOG, 0, "cannot set slave id");
			modbus_close(_mb);
			modbus_free(_mb);
			_mb = NULL;
			throw std::runtime_error("cannot set device id");
		}
	}

	// run the thread
	debug(LOG_DEBUG, DEBUG_LOG, 0, "launching the meter thread");
	std::unique_lock<std::mutex>	lock(_mutex);
	_active = true;
	_thread = std::thread(launch, this);
}

/**
 * \brief Destroy the meter class
 */
modbus_meter::~modbus_meter() {
	stopthread();
	// clean up the connection
	if (_mb) {
		debug(LOG_DEBUG, DEBUG_LOG, 0, "destroy the modbus context");
		modbus_close(_mb);
		modbus_free(_mb);
	}
}


#define	ALE3_FIRMWARE_VERSION		1
#define	ALE3_NUMBER_OF_REGISTERS	2
#define	ALE3_NUMBER_OF_FLAGS		3
#define	ALE3_BAUDRATE_HIGH		4
#define	ALE3_BAUDRATE_LOW		5
#define	ALE3_ASN1			7
#define	ALE3_ASN2			8
#define	ALE3_ASN3			9
#define	ALE3_ASN4			10
#define	ALE3_ASN5			11
#define	ALE3_ASN6			12
#define	ALE3_ASN7			13
#define	ALE3_ASN8			14
#define	ALE3_HW_VERSION			15
#define	ALE3_SERIAL_LOW			16
#define	ALE3_SERIAL_HIGH		17
#define	ALE3_STATUS			22
#define	ALE3_RESPONSE_TIMEOUT		23
#define	ALE3_MODBUS_ADDRESS		24
#define	ALE3_ERROR			25
#define	ALE3_TARIFF			27
#define	ALE3_TOTAL_TARIFF1_HIGH		28
#define	ALE3_TOTAL_TARIFF1_LOW		29
#define	ALE3_PARTIAL_TARIFF1_HIGH	30
#define	ALE3_PARTIAL_TARIFF1_LOW	31
#define	ALE3_TOTAL_TARIFF2_HIGH		32
#define	ALE3_TOTAL_TARIFF2_LOW		33
#define	ALE3_PARTIAL_TARIFF2_HIGH	34
#define	ALE3_PARTIAL_TARIFF2_LOW	35
#define	ALE3_URMS_PHASE1		36
#define	ALE3_IRMS_PHASE1		37
#define	ALE3_PRMS_PHASE1		38
#define	ALE3_QRMS_PHASE1		39
#define	ALE3_COSPHI_PHASE1		40
#define	ALE3_URMS_PHASE2		41
#define	ALE3_IRMS_PHASE2		42
#define	ALE3_PRMS_PHASE2		43
#define	ALE3_QRMS_PHASE2		44
#define	ALE3_COSPHI_PHASE2		45
#define	ALE3_URMS_PHASE3		46
#define	ALE3_IRMS_PHASE3		47
#define	ALE3_PRMS_PHASE3		48
#define	ALE3_QRMS_PHASE3		49
#define	ALE3_COSPHI_PHASE3		50
#define	ALE3_PRMS_TOTAL			51
#define	ALE3_QRMS_TOTAL			52

/**
 * \brief integrate all the information from the meter
 *
 * This method integrates until the end of the current minute and
 * then extrapolates the power to the full minute
 */
message	modbus_meter::integrate() {
	std::unique_lock<std::mutex>	lock(_mutex);
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start integrating");

	// compute the time for the next minute interval
	std::chrono::system_clock::time_point	start
		= std::chrono::system_clock::now();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start is %ld", start.time_since_epoch().count());

	debug(LOG_DEBUG, DEBUG_LOG, 0, "seconds: %d", start.time_since_epoch() * std::chrono::system_clock::period::num / std::chrono::system_clock::period::den);

	// round down to the start of the minute
	std::chrono::seconds	startduration
		= std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::duration_cast<std::chrono::minutes>(
			start.time_since_epoch()));
	debug(LOG_DEBUG, DEBUG_LOG, 0, "startduration is %ld",
		startduration.count());
	start = std::chrono::time_point<std::chrono::system_clock,
		std::chrono::seconds>(startduration);
	
	auto	end = start + std::chrono::seconds(60);
	debug(LOG_DEBUG, DEBUG_LOG, 0, "start: %ld, end: %ld",
		start.time_since_epoch().count(),
		end.time_since_epoch().count());

	// create the result
	message	result(start);
	std::chrono::system_clock::time_point	previous = start;

	// iterate until the end
	std::chrono::system_clock::time_point	now;
	while ((now = std::chrono::system_clock::now()) < end) {
		// compute the remaining time
		std::chrono::duration<float>	remaining = end - now;

		// compute the largest possible interval we can wait
		if (remaining > _interval) {
			remaining = _interval;
		}

		// wait for the remaining time
		debug(LOG_DEBUG, DEBUG_LOG, 0, "waiting for %.3f",
			remaining.count());
		switch (_signal.wait_for(lock, remaining)) {
		case std::cv_status::timeout:
			//debug(LOG_DEBUG, DEBUG_LOG, 0, "timeout");
			break;
		case std::cv_status::no_timeout:
			// this means we were signaled to interrup
			//debug(LOG_DEBUG, DEBUG_LOG, 0, "wait interrupted");
			std::string	msg("meter thread interrupted");
			debug(LOG_DEBUG, DEBUG_LOG, 0, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
		
		uint16_t	registers[53];
		// read a message
		if (simulate) {
			debug(LOG_DEBUG, DEBUG_LOG, 0, "read simulated data");
			read(registers);
		} else {
			debug(LOG_DEBUG, DEBUG_LOG, 0, "read data from modbus");
			int	step = 10;
			int	maxreg = 52;
			for (int reg = 0; reg < maxreg; reg += step) {
				int	rc;
				int	n = std::min(step, maxreg - reg);
				debug(LOG_DEBUG, DEBUG_LOG, 0,
					"reading %d regs starting from %d",
					n, reg);
				rc = modbus_read_registers(_mb, reg, n,
					registers + reg);
				if (rc == -1) {
					std::string	msg = stringprintf(
						"cannot read registers: %s",
						modbus_strerror(errno));
					debug(LOG_ERR, DEBUG_LOG, 0, "%s",
						msg.c_str());
					throw std::runtime_error(msg);
				}
			}
		}

		// end time for the integration
		std::chrono::duration<float>	delta(std::chrono::system_clock::now() - previous);
		//debug(LOG_DEBUG, DEBUG_LOG, 0, "delta: %.3f", delta.count());
		previous = std::chrono::system_clock::now();
		
		// accumulate the data
		result.accumulate(delta, "urms_phase1",
			1. * registers[ALE3_URMS_PHASE1]);
		result.accumulate(delta, "irms_phase1",
			0.1 * registers[ALE3_IRMS_PHASE1]);
		result.accumulate(delta, "prms_phase1",
			10 * registers[ALE3_PRMS_PHASE1]);
		result.accumulate(delta, "qrms_phase1",
			0.01 * registers[ALE3_QRMS_PHASE1]);
		result.accumulate(delta, "cosphi_phase1",
			0.01 * registers[ALE3_COSPHI_PHASE1]);

		result.accumulate(delta, "urms_phase2",
			1. * registers[ALE3_URMS_PHASE2]);
		result.accumulate(delta, "irms_phase2",
			0.1 * registers[ALE3_IRMS_PHASE2]);
		result.accumulate(delta, "prms_phase2",
			10 * registers[ALE3_PRMS_PHASE2]);
		result.accumulate(delta, "qrms_phase2",
			0.01 * registers[ALE3_QRMS_PHASE2]);
		result.accumulate(delta, "cosphi_phase2",
			0.01 * registers[ALE3_COSPHI_PHASE2]);

		result.accumulate(delta, "urms_phase3",
			1. * registers[ALE3_URMS_PHASE3]);
		result.accumulate(delta, "irms_phase3",
			0.1 * registers[ALE3_IRMS_PHASE3]);
		result.accumulate(delta, "prms_phase3",
			10 * registers[ALE3_PRMS_PHASE3]);
		result.accumulate(delta, "qrms_phase3",
			0.01 * registers[ALE3_QRMS_PHASE3]);
		result.accumulate(delta, "cosphi_phase3",
			0.01 * registers[ALE3_COSPHI_PHASE3]);

		result.accumulate(delta, "prms_total",
			10 * registers[ALE3_PRMS_TOTAL]);
		result.accumulate(delta, "qrms_total",
			0.01 * registers[ALE3_QRMS_TOTAL]);

	}
	debug(LOG_DEBUG, DEBUG_LOG, 0, "integration complete");

	// when we get here, we are at the end of the interval, so we now
	// have to extrapolate to the full minute
	float	d = std::chrono::duration<double>(end - start).count();
	debug(LOG_DEBUG, DEBUG_LOG, 0, "duration was %.6f", d);

	// some entries need averaging
	float	factor = 1. / d;
	result.finalize("urms_phase1", factor);
	result.finalize("irms_phase1", factor);
	result.finalize("prms_phase1", factor);
	result.finalize("qrms_phase1", factor);
	result.finalize("cosphi_phase1", factor);

	result.finalize("urms_phase2", factor);
	result.finalize("irms_phase2", factor);
	result.finalize("prms_phase2", factor);
	result.finalize("qrms_phase2", factor);
	result.finalize("cosphi_phase2", factor);

	result.finalize("urms_phase3", factor);
	result.finalize("prms_phase3", factor);
	result.finalize("irms_phase3", factor);
	result.finalize("qrms_phase3", factor);
	result.finalize("cosphi_phase3", factor);

	result.finalize("prms_total", factor);
	result.finalize("qrms_total", factor);

	// return the message
	return result;
}

/**
 * \brief Read data from simulator
 */
void	modbus_meter::read(unsigned short *registers) {
	memset(registers, 0, 53 * sizeof(short));
	registers[ALE3_FIRMWARE_VERSION] = 11;
	registers[ALE3_NUMBER_OF_REGISTERS] = 52;
	registers[ALE3_NUMBER_OF_FLAGS] = 0;
	registers[ALE3_BAUDRATE_HIGH] = 1;
	registers[ALE3_BAUDRATE_LOW] = 49664;
	registers[ALE3_ASN1] = 0x414c; // AL
	registers[ALE3_ASN2] = 0x4533; // E3
	registers[ALE3_ASN3] = 0x4435; // D5
	registers[ALE3_ASN4] = 0x4644; // FD
	registers[ALE3_ASN5] = 0x3130; // 10
	registers[ALE3_ASN6] = 0x4332; // C2
	registers[ALE3_ASN7] = 0x4130; // A0
	registers[ALE3_ASN8] = 0x3000; // 0
	registers[ALE3_HW_VERSION] = 11;
	registers[ALE3_SERIAL_LOW] = 0;
	registers[ALE3_SERIAL_HIGH] = 0;
	registers[ALE3_STATUS] = 0;
	registers[ALE3_RESPONSE_TIMEOUT] = 0;
	registers[ALE3_MODBUS_ADDRESS] = 47;
	registers[ALE3_ERROR] = 0;
	registers[ALE3_TARIFF] = 4;
	registers[ALE3_TOTAL_TARIFF1_HIGH] = 13;
	registers[ALE3_TOTAL_TARIFF1_LOW] = 60383;
	registers[ALE3_PARTIAL_TARIFF1_HIGH] = 13;
	registers[ALE3_PARTIAL_TARIFF1_LOW] = 60383;
	registers[ALE3_TOTAL_TARIFF2_HIGH] = 13;
	registers[ALE3_TOTAL_TARIFF2_LOW] = 60383;
	registers[ALE3_PARTIAL_TARIFF2_HIGH] = 13;
	registers[ALE3_PARTIAL_TARIFF2_LOW] = 60383;

	registers[ALE3_URMS_PHASE1] = sim.urms_phase1();
	registers[ALE3_IRMS_PHASE1] = sim.irms_phase1();
	registers[ALE3_PRMS_PHASE1] = sim.prms_phase1();
	registers[ALE3_QRMS_PHASE1] = sim.qrms_phase1();
	registers[ALE3_COSPHI_PHASE1] = sim.cosphi_phase1();

	registers[ALE3_URMS_PHASE2] = sim.urms_phase2();
	registers[ALE3_IRMS_PHASE2] = sim.irms_phase2();
	registers[ALE3_PRMS_PHASE2] = sim.prms_phase2();
	registers[ALE3_QRMS_PHASE2] = sim.qrms_phase2();
	registers[ALE3_COSPHI_PHASE2] = sim.cosphi_phase2();

	registers[ALE3_URMS_PHASE3] = sim.urms_phase3();
	registers[ALE3_IRMS_PHASE3] = sim.irms_phase3();
	registers[ALE3_PRMS_PHASE3] = sim.prms_phase3();
	registers[ALE3_QRMS_PHASE3] = sim.qrms_phase3();
	registers[ALE3_COSPHI_PHASE3] = sim.cosphi_phase3();

	registers[ALE3_PRMS_TOTAL] = sim.prms_total();
	registers[ALE3_QRMS_TOTAL] = sim.qrms_total();
}

} // namespace powermeter
