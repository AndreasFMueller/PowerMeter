//
// simulator.h
//
// (c) 2023 Prof Dr Andreas Müller
//
#ifndef _simulator_h
#define _simulator_h

#include <chrono>
#include <random>

namespace powermeter {

class phase {
	std::chrono::system_clock::time_point	_start;
protected:
	float	t(const std::chrono::system_clock::time_point&) const;
private:
	std::default_random_engine	generator;
	std::normal_distribution<float>	distribution;
protected:
	float	random() { return distribution(generator); }
public:
	phase();
	virtual ~phase();
	virtual float	urms(const std::chrono::system_clock::time_point&) = 0;
	virtual float	irms(const std::chrono::system_clock::time_point&) = 0;
	float	prms(const std::chrono::system_clock::time_point& _t);
	virtual float	qrms(const std::chrono::system_clock::time_point&) = 0;
	virtual float	cosphi(const std::chrono::system_clock::time_point&) = 0;
};

class phase1 : public phase {
public:
	phase1();
	float	urms(const std::chrono::system_clock::time_point&);
	float	irms(const std::chrono::system_clock::time_point&);
	float	qrms(const std::chrono::system_clock::time_point&);
	float	cosphi(const std::chrono::system_clock::time_point&);
};

class phase2 : public phase {
	static float	period;
	float	squarewave(const std::chrono::system_clock::time_point&) const;
public:
	phase2();
	float	urms(const std::chrono::system_clock::time_point&);
	float	irms(const std::chrono::system_clock::time_point&);
	float	qrms(const std::chrono::system_clock::time_point&);
	float	cosphi(const std::chrono::system_clock::time_point&);
};

class phase3 : public phase {
	static float	period;
	float	trianglewave(const std::chrono::system_clock::time_point&) const;
public:
	phase3();
	float	urms(const std::chrono::system_clock::time_point&);
	float	irms(const std::chrono::system_clock::time_point&);
	float	qrms(const std::chrono::system_clock::time_point&);
	float	cosphi(const std::chrono::system_clock::time_point&);
};

class simulator {
	unsigned short	_serial[3];
	phase1	p1;
	phase2	p2;
	phase3	p3;
	unsigned short	urms(float) const;
	unsigned short	irms(float) const;
	unsigned short	prms(float) const;
	unsigned short	qrms(float) const;
	unsigned short	cosphi(float) const;
public:
	simulator();
	unsigned short	urms_phase1(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	urms_phase2(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	urms_phase3(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	irms_phase1(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	irms_phase2(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	irms_phase3(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	prms_phase1(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	prms_phase2(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	prms_phase3(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	qrms_phase1(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	qrms_phase2(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	qrms_phase3(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	cosphi_phase1(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	cosphi_phase2(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	cosphi_phase3(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	prms_total(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	unsigned short	qrms_total(const std::chrono::system_clock::time_point _t = std::chrono::system_clock::now());
	const unsigned short	*serial() const { return _serial; }
};

} // namespace powermeter

#endif /* _simulator_h */
