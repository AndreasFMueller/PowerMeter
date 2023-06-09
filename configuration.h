//
// confguration.h -- configuration file format
//
// (c) 2023 Prof Dr Andreas Müller
//
#ifndef _configuration_h
#define _configuration_h

#include <string>
#include <map>

namespace powermeter {

class configuration : public std::map<std::string, std::string> {
public:
	configuration();
	configuration(const std::string& filename);
	const std::string&	stringvalue(const std::string& name) const;
	const std::string&	stringvalue(const std::string& name,
					const std::string& defaultvalue) const;
	int	intvalue(const std::string& name) const;
	int	intvalue(const std::string& name, int defaultvalue) const;
	float	floatvalue(const std::string& name) const;
	float	floatvalue(const std::string& name, float defaultvalue) const;
	bool	boolvalue(const std::string& name) const;
	bool	boolvalue(const std::string& name, bool defaultvalue) const;
	void	set(const std::string& name, const std::string& value);
	void	set(const std::string& name, int value);
	void	set(const std::string& name, float value);
	void	set(const std::string& name, bool value);
};

} // namespace powermeter

#endif /* _configuration_h */
