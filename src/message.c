#include <stdlib.h>
#include <stdio.h>
#include "message.h"
#include "debug.h"

void add_to_message_body_once(DBusMessageIter *ia, DBusSignatureIter *is, va_list *body) {
	char y;
	dbus_bool_t b;
	dbus_int16_t n;
	dbus_uint16_t q;
	dbus_int32_t i;
	dbus_uint32_t u;
	dbus_int64_t x;
	dbus_uint64_t t;
	double d;
	char *s;
	char *o;
	char *g;
	size_t array_size;
	char *variant_signature;
	DBusMessageIter sub_message;
	DBusSignatureIter sub_signature;
	FILE h;

	int type = dbus_signature_iter_get_current_type(is);

	if (type == DBUS_TYPE_VARIANT) {
		variant_signature = (char *)va_arg(*body, const char *);
		dbus_signature_iter_init(&sub_signature, variant_signature);
		dbus_message_iter_open_container(
			ia,
			type,
			dbus_signature_iter_get_signature(&sub_signature),
			&sub_message
		);
		add_to_message_body(&sub_message, &sub_signature, body);
		dbus_message_iter_close_container(ia, &sub_message);
	} else if (dbus_type_is_container(type)) {
		dbus_signature_iter_recurse(is, &sub_signature);
		char *sub_sig = dbus_signature_iter_get_signature(&sub_signature);
		dbus_message_iter_open_container(ia, type, sub_sig, &sub_message);
		switch (type) {
		case DBUS_TYPE_DICT_ENTRY:
		case DBUS_TYPE_STRUCT:
			add_to_message_body(&sub_message, &sub_signature, body);
			break;

		case DBUS_TYPE_ARRAY:
			array_size = va_arg(*body, size_t);
			for (size_t i = 0; i < array_size; ++i) {
				add_to_message_body(&sub_message, &sub_signature, body);
			}
			break;
		}
		dbus_message_iter_close_container(ia, &sub_message);
		free(sub_sig);
	}

	switch (type) {
		/* Primitive types */
		/** Type code marking an 8-bit unsigned integer */
	case DBUS_TYPE_BYTE:
		y = va_arg(*body, int);
		dbus_message_iter_append_basic(ia, type, &y);
		break;
		/** Type code marking a boolean */
	case DBUS_TYPE_BOOLEAN:
		b = va_arg(*body, dbus_bool_t);
		dbus_message_iter_append_basic(ia, type, &b);
		break;
		/** Type code marking a 16-bit signed integer */
	case DBUS_TYPE_INT16:
		n = va_arg(*body, int);
		dbus_message_iter_append_basic(ia, type, &n);
		break;
		/** Type code marking a 16-bit unsigned integer */
	case DBUS_TYPE_UINT16:
		q = va_arg(*body, int);
		dbus_message_iter_append_basic(ia, type, &q);
		break;
		/** Type code marking a 32-bit signed integer */
	case DBUS_TYPE_INT32:
		i = va_arg(*body, dbus_int32_t);
		dbus_message_iter_append_basic(ia, type, &i);
		break;
		/** Type code marking a 32-bit unsigned integer */
	case DBUS_TYPE_UINT32:
		u = va_arg(*body, dbus_uint32_t);
		dbus_message_iter_append_basic(ia, type, &u);
		break;
		/** Type code marking a 64-bit signed integer */
	case DBUS_TYPE_INT64:
		x = va_arg(*body, dbus_int64_t);
		dbus_message_iter_append_basic(ia, type, &x);
		break;
		/** Type code marking a 64-bit unsigned integer */
	case DBUS_TYPE_UINT64:
		t = va_arg(*body, dbus_uint64_t);
		dbus_message_iter_append_basic(ia, type, &t);
		break;
		/** Type code marking an 8-byte double in IEEE 754 format */
	case DBUS_TYPE_DOUBLE:
		d = va_arg(*body, double);
		dbus_message_iter_append_basic(ia, type, &d);
		break;
		/** Type code marking a UTF-8 encoded, nul-terminated Unicode string */
	case DBUS_TYPE_STRING:
		s = (char *)va_arg(*body, const char *);
		dbus_message_iter_append_basic(ia, type, &s);
		break;
		/** Type code marking a D-Bus object path */
	case DBUS_TYPE_OBJECT_PATH:
		o = (char *)va_arg(*body, const char *);
		dbus_message_iter_append_basic(ia, type, &o);
		break;
		/** Type code marking a D-Bus type signature */
	case DBUS_TYPE_SIGNATURE:
		g = (char *)va_arg(*body, const char *);
		dbus_message_iter_append_basic(ia, type, &g);
		break;
		/** Type code marking a unix file descriptor */
	case DBUS_TYPE_UNIX_FD:
		h = va_arg(*body, FILE);
		dbus_message_iter_append_basic(ia, type, &h);
		break;
	}
}

void add_to_message_body(DBusMessageIter *ia, DBusSignatureIter *is, va_list *body) {
	int type;
	while((type = dbus_signature_iter_get_current_type(is)) != DBUS_TYPE_INVALID) {
		add_to_message_body_once(ia, is, body);
		dbus_signature_iter_next(is);
	}
}

dbus_bool_t add_to_message(DBusMessage *message, const char *signature, ...) {
	DBusError error = DBUS_ERROR_INIT;
	dbus_bool_t valid_signature = dbus_signature_validate(signature, &error);

	if(!valid_signature) {
		debug(&error);
		dbus_error_free(&error);
		return FALSE;
	}

	DBusSignatureIter i_signature;
	dbus_signature_iter_init(&i_signature, signature);

	DBusMessageIter i_message;
	dbus_message_iter_init_append(message, &i_message);

	va_list body;
	va_start(body, signature);
	add_to_message_body(&i_message, &i_signature, &body);
	va_end(body);

	return TRUE;
}

dbus_bool_t assert_arg_type(int expected_type, DBusMessageIter *iter) {
	int type = dbus_message_iter_get_arg_type(iter);
	if(type != expected_type) {
		fprintf(stderr, "Expected %c, got %c.\n", expected_type, type);
		return FALSE;
	}
	return TRUE;
}

dbus_bool_t get_basic_arg(int expected_type, DBusMessageIter *iter, void *loc) {
	if(!assert_arg_type(expected_type, iter)) return FALSE;
	dbus_message_iter_get_basic(iter, loc);
	dbus_message_iter_next(iter);
	return TRUE;
}
