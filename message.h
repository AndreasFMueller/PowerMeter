/* 
 * message.h -- Data holder class for the values to be added to the database
 *
 * (c) 2023 Prof Dr Andreas Müller
 */
#ifndef _message_h
#define _message_h

#include <map>
#include <string>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace powermeter {

class message : public std::map<std::string, float> {
	std::chrono::steady_clock::time_point	_when;
public:
	message(const std::chrono::steady_clock::time_point& when);
	const std::chrono::steady_clock::time_point&	when() const;
	void	when(const std::chrono::steady_clock::time_point& w);
	bool	has(const std::string& name);
	void	accumulate(const std::chrono::duration<float>& duration,
			const std::string& name,
			const float value);
	void	finalize(const std::string& name, float factor);
};

class messagequeue : public std::deque<message> {
	bool			_active;
	std::mutex		_mutex;
	std::condition_variable	_signal;
	std::chrono::steady_clock::time_point	_last_submit;
	std::chrono::steady_clock::time_point	_last_extract;
public:
	typedef enum { timeout, terminated } status;
	const std::chrono::steady_clock::time_point&	last_submit() const;
	const std::chrono::steady_clock::time_point&	last_extract() const;
	messagequeue();
	~messagequeue();
	void	submit(const message& m);
	message	extract();
	status	wait(const std::chrono::duration<float>& howlong);
};

} // namespace powermeter

#endif /* _message_h */
