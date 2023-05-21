#
# Makefile for the PowerMeter project
#
# (c) 2023 Prof Dr Andreas Müller
# 
EXTRA_DIST = SBC_Energymeter-ALE3-Modbus_eng_tds.pdf meteo.xml

noinst_LTLIBRARIES = libpowermeter.la

libpowermeter_la_SOURCES =						\
	configuration.cpp						\
	database.cpp							\
	debug.c								\
	format.cpp							\
	message.cpp							\
	meter.cpp							\
	simulator.cpp

noinst_HEADERS =							\
	configuration.h							\
	database.h							\
	debug.h								\
	format.h							\
	message.h							\
	meter.h								\
	simulator.h

bin_PROGRAMS = powermeterd

powermeterd_SOURCES = powermeterd.cpp
powermeterd_DEPENDENCIES = libpowermeter.la
powermeterd_LDFLAGS = -L. -lpowermeter

dbhost = antonius
dbname = meteo
dbuser = powermaster
dbpassword = tom,ri5tentR
dbport = 3307

test:	powermeterd powermeter.config
	./powermeterd --foreground \
		--config=powermeter.config \
		--debug

