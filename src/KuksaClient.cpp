/*
 * Copyright (C) 2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>
#include <regex>
#include <iterator>
#include <mutex>

#include "KuksaClient.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;

KuksaClient::KuksaClient(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const KuksaConfig &config) :
	m_config(config)
{
	m_stub = VAL::NewStub(channel);
}

void KuksaClient::get(const std::string &path, GetResponseCallback cb, const bool actuator)
{
	ClientContext *context = new ClientContext();
	if (!context) {
		handleCriticalFailure("Could not create ClientContext");
		return;
	}
	std::string token = m_config.authToken();
	if (!token.empty()) {
		token.insert(0, std::string("Bearer "));
		context->AddMetadata(std::string("authorization"), token);
	}

	GetRequest request;
	auto entry = request.add_entries();
	entry->set_path(path);
	entry->add_fields(Field::FIELD_PATH);	
	if (actuator)
		entry->add_fields(Field::FIELD_ACTUATOR_TARGET);
	else
		entry->add_fields(Field::FIELD_VALUE);

	GetResponse *response = new GetResponse();
	if (!response) {
		handleCriticalFailure("Could not create GetResponse");
		return;
	}

	// NOTE: Using ClientUnaryReactor instead of the shortcut method
	//       would allow getting detailed errors.
	m_stub->async()->Get(context, &request, response,
			     [this, cb, context, response](Status s) {
				     if (s.ok())
					     handleGetResponse(response, cb);
				     delete response;
				     delete context;
			     });
}

// Since a set request needs a Datapoint with the appropriate type value,
// checking the signal metadata to get the type would be a requirement for
// a generic set call that takes a string as argument.  For now, assume
// that set with a string is specifically for a signal of string type.

void KuksaClient::set(const std::string &path, const std::string &value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_string(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const bool value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_bool_(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const int8_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_int32(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const int16_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_int32(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const int32_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_int32(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const int64_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_int64(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const uint8_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_uint32(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const uint16_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_uint32(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const uint32_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_uint32(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const uint64_t value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_uint64(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const float value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_float_(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::set(const std::string &path, const double value, SetResponseCallback cb, const bool actuator)
{
	Datapoint dp;
	dp.set_double_(value);
	set(path, dp, cb, actuator);
}

void KuksaClient::subscribe(const std::string &path,
			    SubscribeResponseCallback cb,
			    const bool actuator,
			    SubscribeDoneCallback done_cb)
{
	SubscribeRequest *request = new SubscribeRequest();
	if (!request) {
		handleCriticalFailure("Could not create SubscribeRequest");
		return;
	}

	auto entry = request->add_entries();
	entry->set_path(path);
	entry->add_fields(Field::FIELD_PATH);	
	if (actuator)
		entry->add_fields(Field::FIELD_ACTUATOR_TARGET);
	else
		entry->add_fields(Field::FIELD_VALUE);

	subscribe(request, cb, done_cb);
}

void KuksaClient::subscribe(const std::map<std::string, bool> signals,
			    SubscribeResponseCallback cb,
			    SubscribeDoneCallback done_cb)
{
	SubscribeRequest *request = new SubscribeRequest();
	if (!request) {
		handleCriticalFailure("Could not create SubscribeRequest");
		return;
	}

	for(auto it = signals.cbegin(); it != signals.cend(); ++it) {
		auto entry = request->add_entries();
		entry->set_path(it->first);
		entry->add_fields(Field::FIELD_PATH);	
		if (it->second)
			entry->add_fields(Field::FIELD_ACTUATOR_TARGET);
		else
			entry->add_fields(Field::FIELD_VALUE);
	}

	subscribe(request, cb, done_cb);
}

void KuksaClient::subscribe(const SubscribeRequest *request,
			    SubscribeResponseCallback cb,
			    SubscribeDoneCallback done_cb)
{
	if (!(request && cb))
		return;

	class Reader : public grpc::ClientReadReactor<SubscribeResponse> {
	public:
		Reader(VAL::Stub *stub,
		       KuksaClient *client,
		       KuksaConfig &config,
		       const SubscribeRequest *request,
		       SubscribeResponseCallback cb,
		       SubscribeDoneCallback done_cb):
			client_(client),
			config_(config),
			request_(request),
			cb_(cb),
			done_cb_(done_cb) {
			std::string token = config_.authToken();
			if (!token.empty()) {
				token.insert(0, std::string("Bearer "));
				context_.AddMetadata(std::string("authorization"), token);
			}
			stub->async()->Subscribe(&context_, request, this);
			StartRead(&response_);
			StartCall();
		}
		void OnReadDone(bool ok) override {
			std::unique_lock<std::mutex> lock(mutex_);
			if (ok) {
				if (client_)
					client_->handleSubscribeResponse(&response_, cb_);
				StartRead(&response_);
			}
		}
		void OnDone(const Status& s) override {
			status_ = s;
			if (client_) {
				if (config_.verbose() > 1)
					std::cerr << "KuksaClient::subscribe::Reader done" << std::endl;
				client_->handleSubscribeDone(request_, status_, done_cb_);
			}

			// gRPC engine is done with us, safe to self-delete
			delete request_;
			delete this;
		}

	private:
		KuksaClient *client_;
		KuksaConfig config_;
		const SubscribeRequest *request_;
		SubscribeResponseCallback cb_;
		SubscribeDoneCallback done_cb_;

		ClientContext context_;
		SubscribeResponse response_;
		std::mutex mutex_;
		Status status_;
	};
	Reader *reader = new Reader(m_stub.get(), this, m_config, request, cb, done_cb);
	if (!reader)
		handleCriticalFailure("Could not create Subscribe reader");
}

// Private

void KuksaClient::set(const std::string &path, const Datapoint &dp, SetResponseCallback cb, const bool actuator)
{
	ClientContext *context = new ClientContext();
	if (!context) {
		handleCriticalFailure("Could not create ClientContext");
		return;
	}
	std::string token = m_config.authToken();
	if (!token.empty()) {
		token.insert(0, std::string("Bearer "));
		context->AddMetadata(std::string("authorization"), token);
	}

	SetRequest request;
	auto update = request.add_updates();
	auto entry = update->mutable_entry();
	entry->set_path(path);
	if (actuator) {
		auto target = entry->mutable_actuator_target();
		*target = dp;
		update->add_fields(Field::FIELD_ACTUATOR_TARGET);
	} else {
		auto value = entry->mutable_value();
		*value = dp;
		update->add_fields(Field::FIELD_VALUE);		
	}

	SetResponse *response = new SetResponse();
	if (!response) {
		handleCriticalFailure("Could not create SetResponse");
		delete context;
		return;
	}

	// NOTE: Using ClientUnaryReactor instead of the shortcut method
	//       would allow getting detailed errors.
	m_stub->async()->Set(context, &request, response,
				       [this, cb, context, response](Status s) {
					       if (s.ok())
						       handleSetResponse(response, cb);
					       delete response;
					       delete context;
				       });
}

void KuksaClient::handleGetResponse(const GetResponse *response, GetResponseCallback cb)
{
	if (!(response && response->entries_size() && cb))
		return;

	for (auto it = response->entries().begin(); it != response->entries().end(); ++it) {
		// We expect paths in the response entries
		if (!it->path().size())
			continue;

		Datapoint dp;
		if (it->has_actuator_target())
			dp = it->actuator_target();
		else
			dp = it->value();
		cb(it->path(), dp);
	}
}

void KuksaClient::handleSetResponse(const SetResponse *response, SetResponseCallback cb)
{
	if (!(response && response->errors_size() && cb))
		return;

	for (auto it = response->errors().begin(); it != response->errors().end(); ++it) {
		cb(it->path(), it->error());
	}

}

void KuksaClient::handleSubscribeResponse(const SubscribeResponse *response, SubscribeResponseCallback cb)
{
	if (!(response && response->updates_size() && cb))
		return;

	for (auto it = response->updates().begin(); it != response->updates().end(); ++it) {
		// We expect entries that have paths in the response
		if (!(it->has_entry() && it->entry().path().size()))
			continue;

		auto entry = it->entry();
		if (m_config.verbose())
			std::cout << "KuksaClient::handleSubscribeResponse: got value for " << entry.path() << std::endl;

		Datapoint dp;
		if (entry.has_actuator_target())
			dp = entry.actuator_target();
		else
			dp = entry.value();

		cb(entry.path(), dp);
	}
}

void KuksaClient::handleSubscribeDone(const SubscribeRequest *request,
				      const Status &status,
				      SubscribeDoneCallback cb)
{
	if (cb)
		cb(request, status);
}

void KuksaClient::handleCriticalFailure(const std::string &error)
{
	if (error.size())
		std::cerr << error << std::endl;
	exit(1);
}

