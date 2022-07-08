#include "handler.h"
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gunixoutputstream.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "message.h"

const char *SERVER_NAME = "I Spy Notify";
const char *SERVER_VENDOR = "I Spy Notify";
const char *SERVER_VERSION = "0.0.0";
const char *SERVER_SPEC_VERSION = "1.2";

json_object *get_base64_from_path(const char *path) {
	json_object *out;
	gchar *bytes;
	gsize size;
	gchar *base64;
	g_file_get_contents(path, &bytes, &size, NULL);
	base64 = g_base64_encode((const guchar *)bytes, size);
	out = json_object_new_string(base64);
	g_free(base64);
	g_free(bytes);
	return out;
}

json_object *get_base64_from_pixbuf(GdkPixbuf *buf) {
	json_object *out;
	gsize png_size;
	gchar *png_bytes;
	gchar *png_base64;
	gdk_pixbuf_save_to_buffer(buf, &png_bytes, &png_size, "png", NULL, NULL);
	png_base64 = g_base64_encode((const guchar *)png_bytes, png_size);
	out = json_object_new_string(png_base64);
	g_free(png_base64);
	g_free(png_bytes);
	return out;
}

json_object *get_path_from_pixbuf(GdkPixbuf *buf) {
	json_object *out;
	char *path = g_build_filename(g_get_tmp_dir(), "i-spy-notify-XXXXXX.png", NULL);
	gint fd = g_mkstemp(path);
	GOutputStream *stream = g_unix_output_stream_new(fd, TRUE);
	gdk_pixbuf_save_to_stream(buf, stream, "png", NULL, NULL, NULL);
	out = json_object_new_string(path);
	g_object_unref(stream);
	g_free(path);
	return out;
}

dbus_bool_t get_standard_hint(DBusMessageIter *iter, json_object *hints) {
	DBusMessageIter value;
	char *key;
	dbus_message_iter_get_basic(iter, &key);
	dbus_message_iter_next(iter);
	dbus_message_iter_recurse(iter, &value);

	if (
		!strcmp(key, "category") ||
		!strcmp(key, "desktop-entry") ||
		!strcmp(key, "image-path") ||
		!strcmp(key, "sound-file") ||
		!strcmp(key, "sound-name")
	) {
		// String
		char *v;
		dbus_message_iter_get_basic(&value, &v);
		json_object_object_add(hints, key, json_object_new_string(v));
	} else if (
		!strcmp(key, "action-icons") ||
		!strcmp(key, "resident") ||
		!strcmp(key, "suppress-sound") ||
		!strcmp(key, "transient")
	) {
		// Boolean
		dbus_bool_t v;
		dbus_message_iter_get_basic(&value, &v);
		json_object_object_add(hints, key, json_object_new_boolean(v));
	} else if (
		!strcmp(key, "x") ||
		!strcmp(key, "y")
	) {
		// Int32
		dbus_int32_t v;
		dbus_message_iter_get_basic(&value, &v);
		json_object_object_add(hints, key, json_object_new_int(v));
	} else if (!strcmp(key, "urgency")) {
		// Byte
		char v;
		dbus_message_iter_get_basic(&value, &v);
		json_object_object_add(hints, key, json_object_new_int(v));
	} else if (!strcmp(key, "image-data")) {
		DBusMessageIter sub;
		DBusMessageIter image_data;
		json_object *image;
		dbus_int32_t width;
		dbus_int32_t height;
		dbus_int32_t rowstride;
		dbus_bool_t has_alpha;
		dbus_int32_t bits_per_sample;
		dbus_int32_t channels;
		int bytes_size;
		char *bytes;
		GdkPixbuf *buf;
		image = json_object_new_object();
		json_object_object_add(hints, key, image);
		dbus_message_iter_recurse(&value, &sub);
		if (!(
			get_basic_arg(DBUS_TYPE_INT32, &sub, &width) &&
			get_basic_arg(DBUS_TYPE_INT32, &sub, &height) &&
			get_basic_arg(DBUS_TYPE_INT32, &sub, &rowstride) &&
			get_basic_arg(DBUS_TYPE_BOOLEAN, &sub, &has_alpha) &&
			get_basic_arg(DBUS_TYPE_INT32, &sub, &bits_per_sample) &&
			get_basic_arg(DBUS_TYPE_INT32, &sub, &channels)
		)) {
			return FALSE;
		}
		dbus_message_iter_recurse(&sub, &image_data);
		dbus_message_iter_get_fixed_array(&image_data, &bytes, &bytes_size);
		buf = gdk_pixbuf_new_from_data(
			(const guchar *)bytes,
			GDK_COLORSPACE_RGB,
			has_alpha,
			bits_per_sample,
			width,
			height,
			rowstride,
			NULL,
			NULL
		);
		json_object_object_add(image, "png", get_base64_from_pixbuf(buf));
		json_object_object_add(image, "path", get_path_from_pixbuf(buf));
		g_object_unref(buf);
	}

	return TRUE;
}

json_object *get_notification(DBusMessage *message) {
	static GtkIconTheme *theme = NULL;
	if (theme == NULL) {
		theme = gtk_icon_theme_get_default();
	}

	json_object *data = json_object_new_object();
	char *app_name;
	dbus_uint32_t replaces_id;
	char *app_icon;
	char *summary;
	char *body;
	json_object *actions = json_object_new_array();
	json_object *hints = json_object_new_object();
	json_object *image = json_object_new_object();
	dbus_int32_t expire_timeout;
	DBusMessageIter iter;
	dbus_message_iter_init(message, &iter);

	if (!(
		get_basic_arg(DBUS_TYPE_STRING, &iter, &app_name) &&
		get_basic_arg(DBUS_TYPE_UINT32, &iter, &replaces_id) &&
		get_basic_arg(DBUS_TYPE_STRING, &iter, &app_icon) &&
		get_basic_arg(DBUS_TYPE_STRING, &iter, &summary) &&
		get_basic_arg(DBUS_TYPE_STRING, &iter, &body)
	)) {
		return NULL;
	}

	{
		DBusMessageIter sub;
		dbus_message_iter_recurse(&iter, &sub);
		while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
			char *action;
			dbus_message_iter_get_basic(&sub, &action);
			json_object_array_add(actions, json_object_new_string(action));
			dbus_message_iter_next(&sub);
		}
	}
	dbus_message_iter_next(&iter);

	{
		DBusMessageIter sub;
		dbus_message_iter_recurse(&iter, &sub);
		while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
			DBusMessageIter dict;
			dbus_message_iter_recurse(&sub, &dict);
			get_standard_hint(&dict, hints);
			dbus_message_iter_next(&sub);
		}
	}
	dbus_message_iter_next(&iter);

	dbus_message_iter_get_basic(&iter, &expire_timeout);
	dbus_message_iter_next(&iter);

	{
		json_object *image_data = json_object_object_get(hints, "image-data");
		json_object *image_path = json_object_object_get(hints, "image-path");
		if (json_object_is_type(image_data, json_type_object)) {
			json_object_object_add(
				image,
				"base64",
				json_object_get(json_object_object_get(image_data, "png"))
			);
			json_object_object_add(
				image,
				"path",
				json_object_get(json_object_object_get(image_data, "path"))
			);
		} else if (json_object_is_type(image_path, json_type_string)) {
			json_object_object_add(
				image,
				"base64",
				get_base64_from_path(json_object_get_string(image_path))
			);
			json_object_object_add(
				image,
				"path",
				json_object_get(image_path)
			);
		} else if (strcmp(app_icon, "") != 0) {
			GtkIconInfo *info = gtk_icon_theme_lookup_icon(theme, app_icon, 64, 0);
			if (info == NULL) {
				json_object_object_add(image, "base64", get_base64_from_path(app_icon));
				json_object_object_add(image, "path", json_object_new_string(app_icon));
			} else {
				const gchar *path = gtk_icon_info_get_filename(info);
				if (path == NULL) {
					GdkPixbuf *buf = gtk_icon_info_load_icon(info, NULL);
					json_object_object_add(image, "base64", get_base64_from_pixbuf(buf));
					json_object_object_add(image, "path", get_path_from_pixbuf(buf));
					g_object_unref(buf);
				} else {
					json_object_object_add(image, "base64", get_base64_from_path(path));
					json_object_object_add(image, "path", json_object_new_string(path));
				}
				g_object_unref(info);
			}
		}
	}

	json_object_object_add(data, "app_name", json_object_new_string(app_name));
	json_object_object_add(data, "replaces_id", json_object_new_uint64(replaces_id));
	json_object_object_add(data, "app_icon", json_object_new_string(app_icon));
	json_object_object_add(data, "summary", json_object_new_string(summary));
	json_object_object_add(data, "body", json_object_new_string(body));
	json_object_object_add(data, "actions", actions);
	json_object_object_add(data, "hints", hints);
	json_object_object_add(data, "expire_timeout", json_object_new_int(expire_timeout));
	json_object_object_add(data, "image", image);

	return data;
}

json_object *nested_object_get(json_object *obj, json_object *keys) {
	json_object *ptr = obj;
	for (size_t i = 0; i < json_object_array_length(keys); ++i) {
		json_object *key = json_object_array_get_idx(keys, i);
		if (json_object_get_type(key) == json_type_string) {
			ptr = json_object_object_get(ptr, json_object_get_string(key));
		} else if (json_object_get_type(key) == json_type_int) {
			ptr = json_object_array_get_idx(ptr, json_object_get_int64(key));
		}
	}
	return ptr;
}

void make_arg_list(json_object *notification, json_object *command, size_t *argc, const char ***argv) {
	*argc = json_object_array_length(command);
	*argv = (const char **)malloc((*argc + 1) * sizeof(char *));
	for (size_t j = 0; j < *argc; ++j) {
		json_object *arg = json_object_array_get_idx(command, j);
		if (json_object_get_type(arg) == json_type_array) {
			json_object *value = nested_object_get(notification, arg);
			if (json_object_get_type(value) == json_type_string) {
				(*argv)[j] = json_object_get_string(value);
			} else {
				(*argv)[j] = json_object_to_json_string(value);
			}
		} else if (json_object_get_type(arg) == json_type_string) {
			(*argv)[j] = json_object_get_string(arg);
		} else {
		}
	}
	(*argv)[*argc] = NULL;
}

void run_hook(json_object *hook, json_object *notification) {
	const char **argv;
	size_t argc;
	json_object *command = json_object_object_get(hook, "command");
	json_object *arguments = json_object_object_get(hook, "arguments");
	json_object *block = json_object_object_get(hook, "block");
	json_object *shell = json_object_object_get(hook, "shell");
	json_object *exec_command = json_object_new_array();
	if (
		json_object_is_type(shell, json_type_boolean) &&
		json_object_get_boolean(shell)
	) {
		json_object_array_add(exec_command, json_object_new_string("sh"));
		json_object_array_add(exec_command, json_object_new_string("-c"));
		json_object_array_add(exec_command, json_object_get(command));
		json_object_array_add(exec_command, json_object_new_string("sh"));
	} else {
		json_object_array_add(exec_command, json_object_get(command));
	}

	for (size_t i = 0; i < json_object_array_length(arguments); ++i) {
		json_object_array_add(
			exec_command,
			json_object_get(json_object_array_get_idx(arguments, i))
		);
	}
	make_arg_list(notification, exec_command, &argc, &argv);

	int pid = fork();
	if (pid) {
		if (
			json_object_is_type(block, json_type_boolean) &&
			json_object_get_boolean(block)
		) {
			int status;
			waitpid(pid, &status, 0);
		}
	} else {
		execvp(argv[0], (char *const *)argv);
	}
	json_object_put(exec_command);
	free(argv);
}

DBusHandlerResult handler(DBusConnection *conn, DBusMessage *message, void *user_data) {
	struct HandlerState *state = (struct HandlerState *)user_data;

	int message_type = dbus_message_get_type(message);
	if (
		message_type != DBUS_MESSAGE_TYPE_METHOD_CALL &&
		message_type != DBUS_MESSAGE_TYPE_SIGNAL
	) {
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	const char *interface = dbus_message_get_interface(message);
	const char *member = dbus_message_get_member(message);
	if (strcmp("org.freedesktop.Notifications", interface) != 0) {
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!strcmp("Notify", member)) {
		if (state->is_server) {
			++state->last_notification_id;
			DBusMessage *r = dbus_message_new_method_return(message);
			add_to_message(r, "u", state->last_notification_id);
			dbus_connection_send(conn, r, NULL);
			dbus_message_unref(r);
		}

		json_object *notification = get_notification(message);
		json_object *hooks = json_object_object_get(state->options, "hooks");
		for (size_t i = 0; i < json_object_array_length(hooks); ++i) {
			json_object *hook = json_object_array_get_idx(hooks, i);
			run_hook(hook, notification);
		}
		json_object_put(notification);
	} else if (!strcmp("NotificationClosed", member)) {
	} else if (!strcmp("GetServerInformation", member)) {
		if (state->is_server) {
			DBusMessage *r = dbus_message_new_method_return(message);
			add_to_message(
				r, "ssss",
				SERVER_NAME,
				SERVER_VENDOR,
				SERVER_VERSION,
				SERVER_SPEC_VERSION
			);
			dbus_connection_send(conn, r, NULL);
			dbus_message_unref(r);
		}
	} else if (!strcmp("GetCapabilities", member)) {
		if (state->is_server) {
			DBusMessage *r = dbus_message_new_method_return(message);
			add_to_message(
				r, "as",
				6,
				"actions",
				"body",
				"body-hyperlinks",
				"body-images",
				"body-markup",
				"persistence"
			);
			dbus_connection_send(conn, r, NULL);
			dbus_message_unref(r);
		}
	} else if (!strcmp("CloseNotification", member)) {
		if (state->is_server) {
			DBusMessage *r = dbus_message_new_method_return(message);
			dbus_connection_send(conn, r, NULL);
			dbus_message_unref(r);
		}
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}
