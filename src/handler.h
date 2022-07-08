#ifndef HANDLER_H
#define HANDLER_H

#include <dbus/dbus.h>
#include <json-c/json.h>

struct HandlerState {
	json_object *options;
	dbus_bool_t is_server;
	dbus_uint32_t last_notification_id;
};

extern const char *SERVER_NAME;
extern const char *SERVER_VENDOR;
extern const char *SERVER_VERSION;
extern const char *SERVER_SPEC_VERSION;

DBusHandlerResult handler(DBusConnection *conn, DBusMessage *message, void *user_data);

#endif
