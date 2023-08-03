#
# Makefile for the PowerMeter project
#
# (c) 2023 Prof Dr Andreas Müller
# 
EXTRA_DIST = SBC_Energymeter-ALE3-Modbus_eng_tds.pdf meteo.xml

noinst_LTLIBRARIES = libpowermeter.la

libpowermeter_la_SOURCES =						\
	ale3_meter.cpp							\
	configuration.cpp						\
	database.cpp							\
	debug.cpp							\
	format.cpp							\
	message.cpp							\
	meter.cpp							\
	meterfactory.cpp						\
	modbus_meter.cpp						\
	simulator.cpp							\
	solivia_meter.cpp

noinst_HEADERS =							\
	ale3_meter.h							\
	configuration.h							\
	database.h							\
	debug.h								\
	format.h							\
	message.h							\
	meter.h								\
	meterfactory.h							\
	modbus_meter.h							\
	simulator.h							\
	solivia_meter.h

bin_PROGRAMS = powermeterd

powermeterd_SOURCES = powermeterd.cpp
powermeterd_DEPENDENCIES = libpowermeter.la
powermeterd_LDFLAGS = -L. -lpowermeter

test:	powermeterd powermeter.config
	./powermeterd --foreground \
		--config=/usr/local/etc/solivia.config \
		--debug

salidomo:	powermeterd salidomo.config
	./powermeterd --foreground \
		--config=./salidomo.config \
		--debug

