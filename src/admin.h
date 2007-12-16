#ifndef __CTRLPROXY_ADMIN_H__
#define __CTRLPROXY_ADMIN_H__

/**
 * @file
 * @brief Administration interface
 */

struct admin_handle;

typedef struct admin_handle *admin_handle;

typedef void (*admin_command_handler) (admin_handle, char **, void *userdata);

/**
 * Handle used in the administration subsystem.
 */
struct admin_handle
{
	struct global *global;
	struct client *client;
	struct irc_network *network;
	void *user_data;
	void (*send_fn) (struct admin_handle *, const char *data);
};

/**
 * Administration command
 */
struct admin_command {
	char *name;
	admin_command_handler handler;
	void *userdata;
};

/**
 * Register a new administration command
 */
G_MODULE_EXPORT void register_admin_command(const struct admin_command *cmd);

/**
 * Reply to an administration command.
 *
 * @param h admin handle
 * @param fmt printf-style string to send
 */
G_MODULE_EXPORT void admin_out(admin_handle h, const char *fmt, ...);

G_MODULE_EXPORT struct client *admin_get_client(admin_handle h);

G_MODULE_EXPORT struct irc_network *admin_get_network(admin_handle h);

G_MODULE_EXPORT struct global *admin_get_global(admin_handle h);

#endif
