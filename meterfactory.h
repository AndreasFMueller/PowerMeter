//
// meterfactory.h
//
// (c) 2024 Prof Dr Andreas Müller
//
#ifndef _meterfactory_h
#define _meterfactory_h

#include <meter.h>
#include <configuration.h>

namespace powermeter {

class meterfactory {
	const configuration&	_config;
public:
	meterfactory(const configuration& config) : _config(config) { }
	meter	*get(const std::string& metertypename, messagequeue& queue);
};

} // namespace powermeter

#endif /* _meterfactory_h */ 
