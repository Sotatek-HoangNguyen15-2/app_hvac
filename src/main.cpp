/*
 * Copyright (C) 2022,2023 Konsulko Group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <glib.h>
#include <glib-unix.h>
#include <systemd/sd-daemon.h>

#include "HvacService.h"

static gboolean quit_cb(gpointer user_data)
{
	GMainLoop *loop = (GMainLoop*) user_data;

	g_info("Quitting...");

	if (loop)
		g_idle_add(G_SOURCE_FUNC(g_main_loop_quit), loop);
	else
		exit(0);

	return G_SOURCE_REMOVE;
}

int main(int argc, char** argv)
{
	GMainLoop *loop = NULL;
	std::cout << "Hello you are in APP HVAC" << std::endl;

	loop = g_main_loop_new(NULL, FALSE);
	if (!loop) {
		std::cerr << "Could not create GLib event loop" << std::endl;
		exit(1);
	}

	KuksaConfig config("agl-service-hvac");

	g_unix_signal_add(SIGTERM, quit_cb, (gpointer) loop);
	g_unix_signal_add(SIGINT, quit_cb, (gpointer) loop);

	HvacService service(config, loop);

	sd_notify(0, "READY=1");

	g_main_loop_run(loop);

	// Clean up
	g_main_loop_unref(loop);

	return 0;
}
