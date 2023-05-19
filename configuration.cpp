//
// configuration.cpp
//
// (c) 2023 Prof Dr Andreas Müller
//
#include <configuration.h>
#include <debug.h>
#include <format.h>
#include <fstream>
#include <cstring>

namespace powermeter {

configuration::configuration() {
	insert(std::make_pair(std::string("dbhostname"),
		std::string("localhost")));
	insert(std::make_pair(std::string("dbport"),
		std::string("3307")));

	insert(std::make_pair(std::string("meterhostname"),
		std::string("localhost")));
	insert(std::make_pair(std::string("meterport"),
		std::string("1471")));
	insert(std::make_pair(std::string("meterid"),
		std::string("1")));
}

static std::string	ltrim(const std::string& s) {
	size_t	i = s.find_first_not_of(" \t");
	if (i != std::string::npos) {
		return s.substr(i);
	}
	return s;
}

static std::string	rtrim(const std::string& s) {
	size_t	i = s.find_last_not_of(" \t");
	if (i != std::string::npos) {
		return s.substr(0, i+1);
	}
	return s;
}

static std::string	trim(const std::string& s) {
	return ltrim(rtrim(s));
}

configuration::configuration(const std::string& filename) {
	char	buffer[10240];
	std::ifstream	in(filename.c_str());
	in.getline(buffer, sizeof(buffer));
	while (!in.eof()) {
		std::string	l(buffer, strlen(buffer));
		size_t	i = l.find("#");
		if (i != std::string::npos) {
			l = l.substr(0, i-1);
		}
		i = l.find("=");
		if (i != std::string::npos) {
			std::string	key = trim(l.substr(0, i));
			std::string	value = trim(l.substr(i+1));
			debug(LOG_DEBUG, DEBUG_LOG, 0, "add '%s' -> '%s'",
				key.c_str(), value.c_str());
			insert(std::make_pair(key, value));
		}
		in.getline(buffer, sizeof(buffer));
	}
}

const std::string&	configuration::stringvalue(const std::string& name) const {
	auto	i = find(name);
	if (i == end()) {
		std::string	msg = stringprintf("cannot find: %s",
					name.c_str());
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	return i->second;
}

int	configuration::intvalue(const std::string& name) const {
	auto	i = find(name);
	if (i == end()) {
		std::string	msg = stringprintf("cannot find: %s",
					name.c_str());
		debug(LOG_ERR, DEBUG_LOG, 0, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	return std::stoi(i->second);
}

const std::string&	configuration::stringvalue(const std::string& name,
				const std::string& defaultvalue) const {
	auto	i = find(name);
	if (i == end()) {
		return defaultvalue;
	}
	return i->second;
}

int	configuration::intvalue(const std::string& name, int defaultvalue) const {
	auto	i = find(name);
	if (i == end()) {
		return defaultvalue;
	}
	return std::stoi(i->second);
}

void	configuration::set(const std::string& name, const std::string& value) {
	insert(std::make_pair(name, value));
}

void	configuration::set(const std::string& name, int value) {
	insert(std::make_pair(name, std::to_string(value)));
}

} // namespace powermeter
