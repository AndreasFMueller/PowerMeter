/*
 * meter.cpp
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#include <meter.h>
#include <stdexcept>

namespace powermeter {

void	meter::launch(meter *m) {
	try {
		m->run();
	} catch (const std::exception& x) {
		// XXX report the exception
	} catch (...) {
		// XXX report the fact that something was thrown
	}
}

meter::meter(const std::string& hostname, unsigned short port,
	int deviceid, messagequeue& queue)
	: _hostname(hostname), _port(port), _deviceid(deviceid),
	  _queue(queue) {

	// set up the connection
	_mb = NULL;
	_mb = modbus_new_tcp(hostname.c_str(), _port);
	if (NULL == _mb) {
		throw std::runtime_error("cannot create a new modbus context");
	}
	if (-1 == modbus_connect(_mb)) {
		throw std::runtime_error("cannot connect");
	}
	if (-1 == modbus_set_slave(_mb, _deviceid)) {
		throw std::runtime_error("cannot set device id");
	}

	// run the thread
	std::unique_lock<std::mutex>	lock(_mutex);
	_active = true;
	_thread = std::thread(launch, this);
}

meter::~meter() {
	{
		std::unique_lock<std::mutex>	lock(_mutex);
		_active = false;
		_signal.notify_all();
	}
	if (_thread.joinable()) {
		_thread.join();
	}

	// clean up the connection
	if (_mb) {
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
message	meter::integrate() {
	std::unique_lock<std::mutex>	lock(_mutex);

	// compute the time for the next minute interval
	std::chrono::steady_clock::time_point	start
		= std::chrono::steady_clock::now();

	// XXX find the end of the minute
	std::chrono::steady_clock::time_point	end
		= std::chrono::steady_clock::now();

	// create the result
	message	result(end);

	// iterate until the end
	while (std::chrono::steady_clock::now() < end) {
		// when we started the integration
		std::chrono::steady_clock::time_point	previous
			= std::chrono::steady_clock::now();

		// wait a few seconds
		switch (_signal.wait_for(lock, std::chrono::seconds(10))) {
		case std::cv_status::timeout:
			break;
		case std::cv_status::no_timeout:
			// this means we were signaled to interrup
			throw std::runtime_error("interrupted");
			break;
		}
		
		uint16_t	registers[53];
		// read a message
		modbus_read_registers(_mb, 0, 53, registers);

		// end time for the integration
		std::chrono::duration<float>	delta(std::chrono::steady_clock::now() - previous);
		
		// accumulate the data
		result.accumulate(delta, "urms_phase1",
			1. * registers[ALE3_URMS_PHASE1]);
		result.accumulate(delta, "irms_phase1",
			0.1 * registers[ALE3_IRMS_PHASE1]);
		result.accumulate(delta, "prms_phase1",
			0.01 * registers[ALE3_PRMS_PHASE1]);
		result.accumulate(delta, "qrms_phase1",
			0.01 * registers[ALE3_QRMS_PHASE1]);
		result.accumulate(delta, "cosphi_phase1",
			0.01 * registers[ALE3_COSPHI_PHASE1]);

		result.accumulate(delta, "urms_phase2",
			1. * registers[ALE3_URMS_PHASE2]);
		result.accumulate(delta, "irms_phase2",
			0.1 * registers[ALE3_IRMS_PHASE2]);
		result.accumulate(delta, "prms_phase2",
			0.01 * registers[ALE3_PRMS_PHASE2]);
		result.accumulate(delta, "qrms_phase2",
			0.01 * registers[ALE3_QRMS_PHASE2]);
		result.accumulate(delta, "cosphi_phase2",
			0.01 * registers[ALE3_COSPHI_PHASE2]);

		result.accumulate(delta, "urms_phase3",
			1. * registers[ALE3_URMS_PHASE3]);
		result.accumulate(delta, "irms_phase3",
			0.1 * registers[ALE3_IRMS_PHASE3]);
		result.accumulate(delta, "prms_phase3",
			0.01 * registers[ALE3_PRMS_PHASE3]);
		result.accumulate(delta, "qrms_phase3",
			0.01 * registers[ALE3_QRMS_PHASE3]);
		result.accumulate(delta, "cosphi_phase3",
			0.01 * registers[ALE3_COSPHI_PHASE3]);

		result.accumulate(delta, "prms_total",
			0.01 * registers[ALE3_PRMS_TOTAL]);
		result.accumulate(delta, "qrms_total",
			0.01 * registers[ALE3_QRMS_TOTAL]);
	}

	// when we get here, we are at the end of the interval, so we now
	// have to extrapolate to the full minute
	float	d = std::chrono::duration<double>(end - start).count();

	// some entries need averaging
	float	factor = 1. / d;
	result.finalize("urms_phase1", factor);
	result.finalize("irms_phase1", factor);
	result.finalize("qrms_phase1", factor);
	result.finalize("cosphi_phase1", factor);
	result.finalize("urms_phase2", factor);
	result.finalize("irms_phase2", factor);
	result.finalize("qrms_phase2", factor);
	result.finalize("cosphi_phase2", factor);
	result.finalize("urms_phase3", factor);
	result.finalize("irms_phase3", factor);
	result.finalize("qrms_phase3", factor);
	result.finalize("cosphi_phase3", factor);
	result.finalize("qrms_total", factor);

	// some entries just need extrapolating
	factor = 60. / d;
	result.finalize("prms_phase1", factor);
	result.finalize("prms_total", factor);

	// return the message
	return result;
}

void	meter::run() {
	while (_active) {
		// during an integration interval
		message	m = integrate();
		
		// submit it to the queue
		_queue.submit(m);
	}
}

} // namespace powermeter
