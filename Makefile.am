#
# Makefile for the PowerMeter project
#
# (c) 2023 Prof Dr Andreas Müller
# 
EXTRA_DIST = SBC_Energymeter-ALE3-Modbus_eng_tds.pdf meteo.xml

noinst_LTLIBRARIES = libpowermeter.la

libpowermeter_la_SOURCES = database.cpp message.cpp meter.cpp debug.c format.cpp

noinst_HEADERS = database.h debug.h message.h meter.h format.h

bin_PROGRAMS = powermeterd

powermeterd_SOURCES = powermeterd.cpp
powermeterd_DEPENDENCIES = libpowermeter.la
powermeterd_LDFLAGS = -L. -lpowermeter

dbhost = antonius
dbname = meteo
dbuser = powermaster
dbpassword = tom,ri5tentR
dbport = 3307

test:	powermeterd
	./powermeterd --foreground \
		--dbhostname=$(dbhost) \
		--dbname=$(dbname) \
		--dbuser=$(dbuser) \
		--dbpassword=$(dbpassword) \
		--dbport=$(dbport) \
		--stationname=Bubental \
		--sensorname=powermeter \
		--meterhostname=test \
		--meterport=1471 \
		--meterid=1 \
		--debug

