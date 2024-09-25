/*
 * Copyright (C) 2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KUKSA_CLIENT_H
#define KUKSA_CLIENT_H

#include <string>
#include <map>
#include <grpcpp/grpcpp.h>
#include "kuksa/val/v1/val.grpc.pb.h"

// Just pull in the whole namespace since Datapoint contains a lot of
// definitions that may potentially be needed.
using namespace kuksa::val::v1;

using grpc::Status;

#include "KuksaConfig.h"

// API response callback types
typedef std::function<void(const std::string &path, const Datapoint &dp)> GetResponseCallback;
typedef std::function<void(const std::string &path, const Error &error)> SetResponseCallback;
typedef std::function<void(const std::string &path, const Datapoint &dp)> SubscribeResponseCallback;
typedef std::function<void(const SubscribeRequest *request, const Status &status)> SubscribeDoneCallback;

// KUKSA.val databroker "VAL" gRPC API client class

class KuksaClient
{
public:
	explicit KuksaClient(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const KuksaConfig &config);

	void get(const std::string &path, GetResponseCallback cb, const bool actuator = false);

	void set(const std::string &path, const std::string &value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const bool value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const int8_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const int16_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const int32_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const int64_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const uint8_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const uint16_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const uint32_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const uint64_t value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const float value, SetResponseCallback cb, const bool actuator = false);
	void set(const std::string &path, const double value, SetResponseCallback cb, const bool actuator = false);

	void subscribe(const std::string &path,
		       SubscribeResponseCallback cb,
		       const bool actuator = false,
		       SubscribeDoneCallback done_cb = nullptr);
	void subscribe(const std::map<std::string, bool> signals,
		       SubscribeResponseCallback cb,
		       SubscribeDoneCallback done_cb = nullptr);
	void subscribe(const SubscribeRequest *request,
		       SubscribeResponseCallback cb,
		       SubscribeDoneCallback done_cb = nullptr);

private:
	KuksaConfig m_config;
	std::shared_ptr<VAL::Stub> m_stub;

	void set(const std::string &path, const Datapoint &dp, SetResponseCallback cb, const bool actuator);

	void handleGetResponse(const GetResponse *response, GetResponseCallback cb);

	void handleSetResponse(const SetResponse *response, SetResponseCallback cb);

	void handleSubscribeResponse(const SubscribeResponse *response, SubscribeResponseCallback cb);

	void handleSubscribeDone(const SubscribeRequest *request, const Status &status, SubscribeDoneCallback cb);

	void handleCriticalFailure(const std::string &error);
	void handleCriticalFailure(const char *error) { handleCriticalFailure(std::string(error)); };
};

#endif // KUKSA_CLIENT_H
