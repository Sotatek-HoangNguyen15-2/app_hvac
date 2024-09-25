/*
 * Copyright (C) 2022,2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "HvacCanHelper.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace property_tree = boost::property_tree;

HvacCanHelper::HvacCanHelper() :
	m_temp_left(21),
	m_temp_right(21),
	m_fan_speed(0),
	m_port("can0"),
	m_config_valid(false),
	m_active(false),
	m_verbose(0)
{
	read_config();

	can_open();
}

HvacCanHelper::~HvacCanHelper()
{
	can_close();
}

void HvacCanHelper::read_config()
{
	// Using a separate configuration file now, it may make sense
	// to revisit this if a workable scheme to handle overriding
	// values for the full demo setup can be come up with.
	std::string config("/etc/xdg/AGL/agl-service-hvac-can.conf");
	char *home = getenv("XDG_CONFIG_HOME");
	if (home) {
		config = home;
		config += "/AGL/agl-service-hvac.conf";
	}

	std::cout << "Using configuration " << config << std::endl;
	property_tree::ptree pt;
	try {
		property_tree::ini_parser::read_ini(config, pt);
	}
	catch (std::exception &ex) {
		// Continue with defaults if file missing/broken
		std::cerr << "Could not read " << config << std::endl;
		m_config_valid = true;
		return;
	}
	const property_tree::ptree &settings =
		pt.get_child("can", property_tree::ptree());

	m_port = settings.get("port", "can0");
	std::stringstream ss;
	ss << m_port;
	ss >> std::quoted(m_port);
	if (m_port.empty()) {
		std::cerr << "Invalid CAN port path" << std::endl;
		return;
	}

	m_verbose = 0;
	std::string verbose = settings.get("verbose", "");
	std::stringstream().swap(ss);
	ss << verbose;
	ss >> std::quoted(verbose);
	if (!verbose.empty()) {
		if (verbose == "true" || verbose == "1")
			m_verbose = 1;
		if (verbose == "2")
			m_verbose = 2;
	}

	m_config_valid = true;
}

void HvacCanHelper::can_open()
{
	if (!m_config_valid)
		return;

	if (m_verbose > 1)
		std::cout << "HvacCanHelper::HvacCanHelper: using port " << m_port << std::endl;

	// Open raw CAN socket
	m_can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (m_can_socket < 0) {
		return;
	}

	// Look up port address
	struct ifreq ifr;
	strcpy(ifr.ifr_name, m_port.c_str());
	if (ioctl(m_can_socket, SIOCGIFINDEX, &ifr) < 0) {
		close(m_can_socket);
		return;
	}

	m_can_addr.can_family = AF_CAN;
	m_can_addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(m_can_socket, (struct sockaddr*) &m_can_addr, sizeof(m_can_addr)) < 0) {
		close(m_can_socket);
		return;
	}

	m_active = true;
	if (m_verbose > 1)
		std::cout << "HvacCanHelper::HvacCanHelper: opened " << m_port << std::endl;
}

void HvacCanHelper::can_close()
{
	if (m_active)
		close(m_can_socket);
}

void HvacCanHelper::set_left_temperature(uint8_t temp)
{
	m_temp_left = temp;
	can_update();
}

void HvacCanHelper::set_right_temperature(uint8_t temp)
{
	m_temp_right = temp;
	can_update();
}

void HvacCanHelper::set_fan_speed(uint8_t speed)
{
	// Scale incoming 0-100 VSS signal to 0-255 to match hardware expectations
	double value = speed * 255.0 / 100.0;
	m_fan_speed = (uint8_t) (value + 0.5);
	can_update();
}

void HvacCanHelper::can_update()
{
	if (!m_active)
		return;

	struct can_frame frame;
	frame.can_id = 0x30;
	frame.can_dlc = 8;
	frame.data[0] = convert_temp(m_temp_left);
	frame.data[1] = convert_temp(m_temp_right);
	frame.data[2] = convert_temp((uint8_t) (((int) m_temp_left + (int) m_temp_right) >> 1));
	frame.data[3] = 0xF0;
	frame.data[4] = m_fan_speed;
	frame.data[5] = 1;
	frame.data[6] = 0;
	frame.data[7] = 0;

	auto written = sendto(m_can_socket,
			      &frame,
			      sizeof(struct can_frame),
			      0,
			      (struct sockaddr*) &m_can_addr,
			      sizeof(m_can_addr));
	if (written < 0) {
		std::cerr << "Write to " << m_port << " failed!" << std::endl;
		close(m_can_socket);
		m_active = false;
	}
}


