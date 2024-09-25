/*
 * Copyright (C) 2022,2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _KUKSA_CONFIG_H
#define _KUKSA_CONFIG_H

#include <string>

class KuksaConfig
{
public:
        explicit KuksaConfig(const std::string &hostname,
			     const unsigned port,
			     const std::string &caCert,
			     const std::string &tlsServerName,
			     const std::string &authToken);
        explicit KuksaConfig(const std::string &appname);
        ~KuksaConfig() {};

	std::string hostname() { return m_hostname; };
	unsigned port() { return m_port; };
	std::string caCert() { return m_caCert; };
	std::string tlsServerName() { return m_tlsServerName; };
	std::string authToken() { return m_authToken; };
	bool valid() { return m_valid; };
	unsigned verbose() { return m_verbose; };

private:
	std::string m_hostname;
	unsigned m_port;
	std::string m_caCert;
	std::string m_tlsServerName;
	std::string m_authToken;
	unsigned m_verbose;
	bool m_valid;

	void readFile(const std::string &filename, std::string &data);
};

#endif // _KUKSA_CONFIG_H
