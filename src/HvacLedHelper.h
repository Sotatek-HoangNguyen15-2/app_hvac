/*
 * Copyright (C) 2022,2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HVAC_LED_HELPER_H
#define _HVAC_LED_HELPER_H

#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>

class HvacLedHelper
{
public:
	HvacLedHelper();

	void set_left_temperature(uint8_t temp);

	void set_right_temperature(uint8_t temp);

private:
	void read_config();

	void led_update();

	std::string m_led_path_red;
	std::string m_led_path_green;
	std::string m_led_path_blue;
	unsigned m_verbose;
	bool m_config_valid;

	uint8_t m_temp_left;
	uint8_t m_temp_right;
};

#endif // _HVAC_LED_HELPER_H
