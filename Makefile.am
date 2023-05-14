#
# Makefile for the PowerMeter project
#
# (c) 2023 Prof Dr Andreas Müller
#
EXTRA_DIST = SBC_Energymeter-ALE3-Modbus_eng_tds.pdf

noinst_LTLIBRARIES = libpowermeter.la

libpowermeter_la_SOURCES = database.cpp message.cpp meter.cpp debug.c

noinst_HEADERS = database.h debug.h message.h meter.h

bin_PROGRAMS = powermeter

powermeter_SOURCES = powermeterd.cpp
powermeter_DEPENDENCIES = libpowermeter.la
powermeter_LDFLAGS = -L. -lpowermeter


