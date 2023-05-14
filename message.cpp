/*
 * message.cpp
 *
 * (c) 2013 Prof Dr Andreas Müller
 */
#include <message.h>

namespace powermeter {

//
// message implementation
//

message::message(const std::chrono::steady_clock::time_point& when) : _when(when) {
}

const std::chrono::steady_clock::time_point&	message::when() const {
	return _when;
}

void	message::when(const std::chrono::steady_clock::time_point& w) {
	_when = w;
}

bool	message::has(const std::string& name) {
	return (find(name) != end());
}

void	message::accumulate(const std::chrono::duration<float>& duration,
		const std::string& name, const float value) {
	//std::chrono::duration<float>	d = duration;
	float	ivalue = value * duration.count();
	std::map<std::string, float>::const_iterator	i = find(name);
	if (i == end()) {
		insert(std::make_pair(name, ivalue));
	} else {
		operator[](name) = i->second + ivalue;
	}
}

//
// messagequeue implementation
//

const std::chrono::steady_clock::time_point&	messagequeue::last_submit() const {
	return _last_submit;
}

const std::chrono::steady_clock::time_point&	messagequeue::last_extract() const {
	return _last_extract;
}

messagequeue::messagequeue() : _active(true),
	_last_submit(std::chrono::steady_clock::now()),
	_last_extract(std::chrono::steady_clock::now()) {
}

messagequeue::~messagequeue() {
	_active = false;
	_signal.notify_all();
}

void	messagequeue::submit(const message& m) {
	std::unique_lock<std::mutex>	lock(_mutex);
	push_front(m);
	_last_submit = std::chrono::steady_clock::now();
}

message	messagequeue::extract() {
	std::unique_lock<std::mutex>	lock(_mutex);
	while (_active) {
		if (size() > 0) {
			auto result = back();
			pop_back();
			_last_extract = std::chrono::steady_clock::now();
			return result;
		}
		_signal.wait(lock);
	}
	throw std::runtime_error("queue terminated");
}

messagequeue::status	messagequeue::wait(const std::chrono::duration<float>& howlong) {
	std::unique_lock<std::mutex>	lock(_mutex);
	while (_active) {
		switch (_signal.wait_for(lock, howlong)) {
		case std::cv_status::timeout:
			return timeout;
		case std::cv_status::no_timeout:
			break;
		}
	}
	return terminated;
}

} // namespace powermeter
