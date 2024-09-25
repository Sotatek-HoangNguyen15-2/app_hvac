/*
 * Copyright (C) 2022,2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <exception>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem.hpp>
#include "KuksaConfig.h"

namespace property_tree = boost::property_tree;
namespace filesystem = boost::filesystem;

#define DEFAULT_CA_CERT_FILE     "/etc/kuksa-val/CA.pem"

inline
void load_string_file(const filesystem::path& p, std::string& str)
{
	std::ifstream file;
	file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	file.open(p, std::ios_base::binary);
	if (file.good()) {
		std::size_t sz = static_cast<std::size_t>(filesystem::file_size(p));
		str.resize(sz, '\0');
		file.read(&str[0], sz);
	} else {
		str.clear();
	}
}

KuksaConfig::KuksaConfig(const std::string &hostname,
			 const unsigned port,
			 const std::string &caCert,
			 const std::string &tlsServerName,
			 const std::string &authToken) :
	m_hostname(hostname),
	m_port(port),
	m_caCert(caCert),
	m_tlsServerName(tlsServerName),
	m_authToken(authToken),
	m_verbose(0),
	m_valid(true)
{
	// Potentially could do some certificate validation here...
}

KuksaConfig::KuksaConfig(const std::string &appname) :
	m_valid(false)
{
	std::string config("/etc/xdg/AGL/");
	config += appname;
	config += ".conf";
	char *home = getenv("XDG_CONFIG_HOME");
	if (home) {
		config = home;
		config += "/AGL/";
		config += appname;
		config += ".conf";
	}

	std::cout << "Using configuration " << config << std::endl;
	property_tree::ptree pt;
	try {
		property_tree::ini_parser::read_ini(config, pt);
	}
	catch (std::exception &ex) {
		std::cerr << "Could not read " << config << std::endl;
		return;
	}
	const property_tree::ptree &settings =
		pt.get_child("kuksa-client", property_tree::ptree());

	m_hostname = settings.get("server", "localhost");
	std::stringstream ss;
	ss << m_hostname;
	ss >> std::quoted(m_hostname);
	if (m_hostname.empty()) {
		std::cerr << "Invalid server hostname" << std::endl;
		return;
	}

	m_port = settings.get("port", 55555);
	if (m_port == 0) {
		std::cerr << "Invalid server port" << std::endl;
		return;
	}

	std::string caCertFileName = settings.get("ca-certificate", DEFAULT_CA_CERT_FILE);
	std::stringstream().swap(ss);
	ss << caCertFileName;
	ss >> std::quoted(caCertFileName);
	if (caCertFileName.empty()) {
		std::cerr << "Invalid CA certificate filename" << std::endl;
		return;
	}
	readFile(caCertFileName, m_caCert);
	if (m_caCert.empty()) {
		std::cerr << "Invalid CA certificate file" << std::endl;
		return;
	}

	m_tlsServerName = settings.get("tls-server-name", "");

	std::string authTokenFileName = settings.get("authorization", "");
	std::stringstream().swap(ss);
	ss << authTokenFileName;
	ss >> std::quoted(authTokenFileName);
	if (authTokenFileName.empty()) {
		std::cerr << "Invalid authorization token filename" << std::endl;
		return;
	}
	readFile(authTokenFileName, m_authToken);
	if (m_authToken.empty()) {
		std::cerr << "Invalid authorization token file" << std::endl;
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

	m_valid = true;
}

// Private

void KuksaConfig::readFile(const std::string &filename, std::string &data)
{
	try {
		load_string_file(filename, data);
	} catch (const std::exception &e) {
		data.clear();
	}
}
