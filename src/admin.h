#ifndef __CTRLPROXY_ADMIN_H__
#define __CTRLPROXY_ADMIN_H__

/**
 * @file
 * @brief Administration interface
 */

#include "client.h"

typedef void (*admin_command_handler) (struct client *c, char **, void *userdata);

/**
 * Administration command
 */
struct admin_command {
	char *name;
	admin_command_handler handler;
	/**
	 * One line description
	 */
	const char *help;
	/**
	 * Extended (multi-line) description
	 */
	const char *help_details;
	void *userdata;
};

/**
 * Register a new administration command
 */
G_MODULE_EXPORT void register_admin_command(const struct admin_command *cmd);

/**
 * Unregister an administration command
 */
G_MODULE_EXPORT void unregister_admin_command(const struct admin_command *cmd);

/**
 * Reply to an administration command.
 *
 * @param c Client to send to
 * @param fmt printf-style string to send
 */
G_MODULE_EXPORT void admin_out(struct client *c, const char *fmt, ...);

#endif
