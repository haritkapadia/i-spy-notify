#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <dbus/dbus.h>
#include <json-c/json.h>
#include <gtk/gtk.h>
#include "debug.h"
#include "message.h"
#include "handler.h"

DBusConnection *connect_to_session_bus() {
	DBusError error = DBUS_ERROR_INIT;
	DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (!conn) debug(&error);
	dbus_error_free(&error);
	return conn;
}

dbus_bool_t become_server(
	DBusConnection *connection,
	struct HandlerState *state,
	DBusObjectPathVTable *server_vtable
) {
	DBusError error = DBUS_ERROR_INIT;
	server_vtable->unregister_function = NULL;
	server_vtable->message_function = handler;
	dbus_bool_t notifications_has_owner = dbus_bus_name_has_owner(
		connection, "org.freedesktop.Notifications", &error
	);
	if (notifications_has_owner) {
		dbus_error_free(&error);
		return FALSE;
	}
	int request_name_result = dbus_bus_request_name(
		connection, "org.freedesktop.Notifications", 0, &error
	);
	if(request_name_result == -1) {
		debug(&error);
		dbus_error_free(&error);
		return FALSE;
	}

	dbus_bool_t registered_on_path = dbus_connection_try_register_object_path(
		connection,
		"/org/freedesktop/Notifications",
		server_vtable,
		state,
		&error
	);
	if(!registered_on_path) {
		if(strcmp(DBUS_ERROR_OBJECT_PATH_IN_USE, error.name) != 0) {
			debug(&error);
			dbus_error_free(&error);
			return FALSE;
		}
	}
	state->is_server = TRUE;
	dbus_error_free(&error);
	return TRUE;
}

void become_monitor(DBusConnection *connection, struct HandlerState *state) {
	state->is_server = FALSE;
	DBusError error = DBUS_ERROR_INIT;
	DBusMessage *message;
	DBusMessage *reply;
	message = dbus_message_new_method_call(
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_MONITORING,
		"BecomeMonitor"
	);
	add_to_message(message, "asu", 0, 0);
	reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);
	if (reply != NULL) dbus_message_unref(reply);
	dbus_message_unref(message);

	dbus_connection_add_filter(connection, handler, (void *)state, NULL);
}

int main(int argc, char **argv) {
	gtk_init(&argc, &argv);
	const gchar *options_file = g_build_filename(
		g_get_user_config_dir(),
		"h-notifications.json",
		NULL
	);
	json_object *options = json_object_from_file(options_file);
	struct HandlerState state;
	DBusObjectPathVTable server_vtable;
	state.options = options;
	state.last_notification_id = 0;
	DBusConnection *conn = connect_to_session_bus();
	if (!become_server(conn, &state, &server_vtable)) {
		become_monitor(conn, &state);
	}
	while(dbus_connection_read_write_dispatch(conn, -1));
	return 0;
}
