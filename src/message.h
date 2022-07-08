#ifndef MESSAGE_H
#define MESSAGE_H

#include <dbus/dbus.h>

void add_to_message_body(DBusMessageIter *ia, DBusSignatureIter *is, va_list *body);
dbus_bool_t add_to_message(DBusMessage *message, const char *signature, ...);
dbus_bool_t get_basic_arg(int expected_type, DBusMessageIter *iter, void *loc);

#endif
