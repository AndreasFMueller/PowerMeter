//
// simulator.cpp
//
// (c) 2023 Prof Dr Andreas Müller
//
#include <simulator.h>
#include <debug.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <format.h>
#include <stdexcept>

namespace powermeter {

//////////////////////////////////////////////////////////////////////
// phase implementation
//////////////////////////////////////////////////////////////////////
phase::phase() : _start(std::chrono::system_clock::now()) {
}

phase::~phase() {
}

float	phase::t(const std::chrono::system_clock::time_point& _t) const {
	return std::chrono::duration<double>(_t - _start).count();
}

float	phase::prms(const std::chrono::system_clock::time_point& _t) {
	return urms(_t) * irms(_t);
}

//////////////////////////////////////////////////////////////////////
// phase1 implementation
//////////////////////////////////////////////////////////////////////

phase1::phase1() {
}

float	phase1::urms(const std::chrono::system_clock::time_point& _t) {
	return 230. + random();
}

float	phase1::irms(const std::chrono::system_clock::time_point& _t) {
	return 1 + 0.5 * sin(M_PI * t(_t) / 3600) + 0.05 * random();
}

float	phase1::qrms(const std::chrono::system_clock::time_point& _t) {
	return 0.02;
}

float	phase1::cosphi(const std::chrono::system_clock::time_point& _t) {
	return 0.97;
}

//////////////////////////////////////////////////////////////////////
// phase2 implementation
//////////////////////////////////////////////////////////////////////

float	phase2::period = 2000;

float	phase2::squarewave(const std::chrono::system_clock::time_point& _t) const {
	float	s = t(_t);
	s -= period * floor(s / period);
	float	l = period/2;
	return (s > l) ? 1 : -1;
}

phase2::phase2() {
}

float	phase2::urms(const std::chrono::system_clock::time_point& _t) {
	return 235. + 5 * squarewave(_t) + random();
}

float	phase2::irms(const std::chrono::system_clock::time_point& _t) {
	return 1.4 + 0.8 * squarewave(_t) + 0.05 * random();
}

float	phase2::qrms(const std::chrono::system_clock::time_point& _t) {
	return 0.05 + 0.3 * (1 + squarewave(_t));
}

float	phase2::cosphi(const std::chrono::system_clock::time_point& _t) {
	return cos(1 + 0.3 * squarewave(_t));
}

//////////////////////////////////////////////////////////////////////
// phase3 implementation
//////////////////////////////////////////////////////////////////////

float	phase3::period = 4711;

float	phase3::trianglewave(const std::chrono::system_clock::time_point& _t) const {
	float	s = t(_t);
	s -= period * floor(s / period);
	float	l = period/2.;
	float	result = 1 - 2 * fabs((s-l)/l);
	return result;
}

phase3::phase3() {
}

float	phase3::urms(const std::chrono::system_clock::time_point& _t) {
	return 235. + 10 * trianglewave(_t) + random();
}

float	phase3::irms(const std::chrono::system_clock::time_point& _t) {
	return 2 * (2 + trianglewave(_t)) + 0.05 * random();
}

float	phase3::qrms(const std::chrono::system_clock::time_point& _t) {
	return 0.1 + 0.05 * trianglewave(_t);
}

float	phase3::cosphi(const std::chrono::system_clock::time_point& _t) {
	return cos(0.5 + trianglewave(_t));
}

//////////////////////////////////////////////////////////////////////
// simulator implementation
//////////////////////////////////////////////////////////////////////

simulator::simulator() {
	int     fd = open("/dev/random", O_RDONLY);
	int     rc = read(fd, _serial, sizeof(_serial));
	if (rc < 0) {
		std::string     msg = stringprintf("read "
			"failed: %s", strerror(errno));
		close(fd);
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	close(fd);
}

unsigned short	simulator::urms(float value) const {
	return (unsigned short)value;
}

unsigned short	simulator::irms(float value) const {
	return (unsigned short)(10 * value);
}

unsigned short	simulator::prms(float value) const {
	return (unsigned short)(0.1 * value);
}

unsigned short	simulator::qrms(float value) const {
	return (unsigned short)(100 * value);
}

unsigned short	simulator::cosphi(float value) const {
	return (unsigned short)(100 * value);
}

// phase 1
unsigned short	simulator::urms_phase1(std::chrono::system_clock::time_point _t) {
	return urms(p1.urms(_t));
}

unsigned short	simulator::irms_phase1(std::chrono::system_clock::time_point _t) {
	return irms(p1.irms(_t));
}

unsigned short	simulator::prms_phase1(std::chrono::system_clock::time_point _t) {
	return prms(p1.prms(_t));
}

unsigned short	simulator::qrms_phase1(std::chrono::system_clock::time_point _t) {
	return qrms(p1.qrms(_t));
}

unsigned short	simulator::cosphi_phase1(std::chrono::system_clock::time_point _t) {
	return cosphi(p1.cosphi(_t));
}

// phase 2
unsigned short	simulator::urms_phase2(std::chrono::system_clock::time_point _t) {
	return urms(p2.urms(_t));
}

unsigned short	simulator::irms_phase2(std::chrono::system_clock::time_point _t) {
	return irms(p2.irms(_t));
}

unsigned short	simulator::prms_phase2(std::chrono::system_clock::time_point _t) {
	return prms(p2.prms(_t));
}

unsigned short	simulator::qrms_phase2(std::chrono::system_clock::time_point _t) {
	return qrms(p2.qrms(_t));
}

unsigned short	simulator::cosphi_phase2(std::chrono::system_clock::time_point _t) {
	return cosphi(p2.cosphi(_t));
}

// phase 3
unsigned short	simulator::urms_phase3(std::chrono::system_clock::time_point _t) {
	return urms(p3.urms(_t));
}

unsigned short	simulator::irms_phase3(std::chrono::system_clock::time_point _t) {
	return irms(p3.irms(_t));
}

unsigned short	simulator::prms_phase3(std::chrono::system_clock::time_point _t) {
	return prms(p3.prms(_t));
}

unsigned short	simulator::qrms_phase3(std::chrono::system_clock::time_point _t) {
	return qrms(p3.qrms(_t));
}

unsigned short	simulator::cosphi_phase3(std::chrono::system_clock::time_point _t) {
	return cosphi(p3.cosphi(_t));
}

// total
unsigned short	simulator::prms_total(std::chrono::system_clock::time_point _t) {
	float	p = p1.prms(_t) + p2.prms(_t) + p3.prms(_t);
	return prms(p);
}

unsigned short	simulator::qrms_total(std::chrono::system_clock::time_point _t) {
	return qrms((1/3.) * (p1.qrms(_t) + p2.qrms(_t) + p3.qrms(_t)));
}

} // namespace powermeter
