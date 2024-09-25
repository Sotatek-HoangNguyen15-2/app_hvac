/*
 * Copyright (C) 2022,2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "HvacService.h"
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

HvacService::HvacService(const KuksaConfig &config, GMainLoop *loop) :
	m_loop(loop),
	m_config(config),
	m_can_helper(),
	m_led_helper()
{
	// Create gRPC channel
	std::string host = m_config.hostname();
	host += ":";
	std::stringstream ss;
	ss << m_config.port();
	host += ss.str();

	std::shared_ptr<grpc::Channel> channel;
	if (!m_config.caCert().empty()) {
		grpc::SslCredentialsOptions options;
		options.pem_root_certs = m_config.caCert();
		if (!m_config.tlsServerName().empty()) {
			grpc::ChannelArguments args;
			auto target = m_config.tlsServerName();
			std::cout << "Overriding TLS target name with " << target << std::endl;
			args.SetString(GRPC_SSL_TARGET_NAME_OVERRIDE_ARG, target);
			channel = grpc::CreateCustomChannel(host, grpc::SslCredentials(options), args);
		} else {
			channel = grpc::CreateChannel(host, grpc::SslCredentials(options));
		}
	} else {
		channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
	}

	// Wait for the channel to be ready
	std::cout << "Waiting for Databroker gRPC channel" << std::endl;
	while (!channel->WaitForConnected(std::chrono::system_clock::now() +
					  std::chrono::milliseconds(500))) ;
	std::cout << "Databroker gRPC channel ready" << std::endl;

	m_broker = new KuksaClient(channel, m_config);
	if (m_broker) {
		// Listen to actuator target updates
		std::map<std::string, bool> signals;
		signals["Vehicle.Cabin.HVAC.Station.Row1.Driver.Temperature"] = true;
		signals["Vehicle.Cabin.HVAC.Station.Row1.Driver.FanSpeed"] = true;
		signals["Vehicle.Cabin.HVAC.Station.Row1.Passenger.Temperature"] = true;
		signals["Vehicle.Cabin.HVAC.Station.Row1.Passenger.FanSpeed"] = true;
		signals["Vehicle.Cabin.HVAC.IsAirConditioningActive"] = true;
		signals["Vehicle.Cabin.HVAC.IsFrontDefrosterActive"] = true;
		signals["Vehicle.Cabin.HVAC.IsRearDefrosterActive"] = true;
		signals["Vehicle.Cabin.HVAC.IsRecirculationActive"] = true;
		m_broker->subscribe(signals,
				    [this](const std::string &path, const Datapoint &dp) {
					    HandleSignalChange(path, dp);
				    },
				    [this](const SubscribeRequest *request, const Status &s) {
					    HandleSubscribeDone(request, s);
				    });
	}
}

HvacService::~HvacService()
{
	delete m_broker;
}

// Private

void HvacService::HandleSignalChange(const std::string &path, const Datapoint &dp)
{
	if (m_config.verbose() > 1)
		std::cout << "HvacService::HandleSignalChange: Value received for " << path << std::endl;

	if (path == "Vehicle.Cabin.HVAC.Station.Row1.Driver.Temperature") {
		if (dp.has_int32()) {
			int temp = dp.int32();
			if (temp >= 0 && temp < 256)
				set_left_temperature(temp);
		}
	} else if (path == "Vehicle.Cabin.HVAC.Station.Row1.Passenger.Temperature") {
		if (dp.has_int32()) {
			int temp = dp.int32();
			if (temp >= 0 && temp < 256)
				set_right_temperature(temp);
		}
	} else if (path == "Vehicle.Cabin.HVAC.Station.Row1.Driver.FanSpeed") {
		if (dp.has_uint32()) {
			int speed = dp.uint32();
			if (speed >= 0 && speed <= 100)
				set_left_fan_speed(speed);
		}
	} else if (path == "Vehicle.Cabin.HVAC.Station.Row1.Passenger.FanSpeed") {
		if (dp.has_uint32()) {
			int speed = dp.uint32();
			if (speed >= 0 && speed <= 100)
				set_right_fan_speed(speed);
		}
	} else if (path == "Vehicle.Cabin.HVAC.IsAirConditioningActive") {
		if (dp.has_bool_()) {
			set_ac_active(dp.bool_());
		}
	} else if (path == "Vehicle.Cabin.HVAC.IsFrontDefrosterActive") {
		if (dp.has_bool_()) {
			set_front_defrost_active(dp.bool_());
		}
	} else if (path == "Vehicle.Cabin.HVAC.IsRearDefrosterActive") {
		if (dp.has_bool_()) {
			set_rear_defrost_active(dp.bool_());
		}
	} else if (path == "Vehicle.Cabin.HVAC.IsRecirculationActive") {
		if (dp.has_bool_()) {
			set_recirculation_active(dp.bool_());
		}
	}
	// else ignore
}

void HvacService::HandleSignalSetError(const std::string &path, const Error &error)
{
	std::cerr << "Error setting " << path << ": " << error.code() << " - " << error.reason() << std::endl;
}

void HvacService::HandleSubscribeDone(const SubscribeRequest *request, const Status &status)
{
	if (m_config.verbose())
		std::cout << "Subscribe status = " << status.error_code() <<
			" (" << status.error_message() << ")" << std::endl;

	if (status.error_code() == grpc::CANCELLED) {
		if (m_config.verbose())
			std::cerr << "Subscribe canceled, assuming shutdown" << std::endl;
		return;
	}

	// Queue up a resubcribe via the GLib event loop callback
	struct resubscribe_data *data = new (struct resubscribe_data);
	if (!data) {
		std::cerr << "Could not create resubcribe_data" << std::endl;
		exit(1);
	}
	data->self = this;
	// Need to copy request since the one we have been handed is from the
	// finished subscribe and will be going away.
	data->request = new SubscribeRequest(*request);
	if (!data->request) {
		std::cerr << "Could not create resubscribe SubscribeRequest" << std::endl;
		exit(1);
	}

	// NOTE: Waiting 100 milliseconds for now; it is possible that some
	//       randomization and/or back-off may need to be added if many
	//       subscribes are active, or switching to some other resubscribe
	//       scheme altogether (e.g. post subscribes to a thread that waits
	//       for the channel to become connected again).
	g_timeout_add_full(G_PRIORITY_DEFAULT,
			   100,
			   resubscribe_cb,
			   data,
			   NULL);
}

void HvacService::Resubscribe(const SubscribeRequest *request)
{
	if (!(m_broker && request))
		return;

	m_broker->subscribe(request,
			    [this](const std::string &path, const Datapoint &dp) {
				    HandleSignalChange(path, dp);
			    },
			    [this](const SubscribeRequest *request, const Status &s) {
				    HandleSubscribeDone(request, s);
			    });
}

// NOTE: The following should perhaps be scheduling work via the GLib
//       main loop to avoid potentially blocking threads from the gRPC
//       pool.

void HvacService::set_left_temperature(uint8_t temp)
{
	m_can_helper.set_left_temperature(temp);
	m_led_helper.set_left_temperature(temp);

	// Push out new value
	m_broker->set("Vehicle.Cabin.HVAC.Station.Row1.Driver.Temperature",
		      (int) temp,
		      [this](const std::string &path, const Error &error) {
			      HandleSignalSetError(path, error);
		      });
}

void HvacService::set_right_temperature(uint8_t temp)
{
	m_can_helper.set_right_temperature(temp);
	m_led_helper.set_right_temperature(temp);

	// Push out new value
	m_broker->set("Vehicle.Cabin.HVAC.Station.Row1.Passenger.Temperature",
		      (int) temp,
		      [this](const std::string &path, const Error &error) {
			      HandleSignalSetError(path, error);
		      });
}

void HvacService::set_left_fan_speed(uint8_t speed)
{
	set_fan_speed(speed);

	// Push out new value
	m_broker->set("Vehicle.Cabin.HVAC.Station.Row1.Driver.FanSpeed",
		      speed,
		      [this](const std::string &path, const Error &error) {
			      HandleSignalSetError(path, error);
		      });
}

void HvacService::set_right_fan_speed(uint8_t speed)
{
	set_fan_speed(speed);

	// Push out new value
	m_broker->set("Vehicle.Cabin.HVAC.Station.Row1.Passenger.FanSpeed",
		      speed,
		      [this](const std::string &path, const Error &error) {
			      HandleSignalSetError(path, error);
		      });
}

void HvacService::set_fan_speed(uint8_t speed)
{
	m_can_helper.set_fan_speed(speed);
}

void HvacService::set_ac_active(bool active)
{
	const std::lock_guard<std::mutex> lock(m_hvac_state_mutex);
	if (m_IsAirConditioningActive != active) {
		m_IsAirConditioningActive = active;

		// Push out new value
		m_broker->set("Vehicle.Cabin.HVAC.IsAirConditioningActive",
			      active,
			      [this](const std::string &path, const Error &error) {
				      HandleSignalSetError(path, error);
			      });
	}
}

void HvacService::set_front_defrost_active(bool active)
{
	const std::lock_guard<std::mutex> lock(m_hvac_state_mutex);
	if (m_IsFrontDefrosterActive != active) {
		m_IsFrontDefrosterActive = active;

		// Push out new value
		m_broker->set("Vehicle.Cabin.HVAC.IsFrontDefrosterActive",
			      active,
			      [this](const std::string &path, const Error &error) {
				      HandleSignalSetError(path, error);
			      });
	}
}

void HvacService::set_rear_defrost_active(bool active)
{
	const std::lock_guard<std::mutex> lock(m_hvac_state_mutex);
	if (m_IsRearDefrosterActive != active) {
		m_IsRearDefrosterActive = active;

		// Push out new value
		m_broker->set("Vehicle.Cabin.HVAC.IsRearDefrosterActive",
			      active,
			      [this](const std::string &path, const Error &error) {
				      HandleSignalSetError(path, error);
			      });
	}
}

void HvacService::set_recirculation_active(bool active)
{
	const std::lock_guard<std::mutex> lock(m_hvac_state_mutex);
	if (m_IsRecirculationActive != active) {
		m_IsRecirculationActive = active;

		// Push out new value
		m_broker->set("Vehicle.Cabin.HVAC.IsRecirculationActive",
			      active,
			      [this](const std::string &path, const Error &error) {
				      HandleSignalSetError(path, error);
			      });
	}
}

