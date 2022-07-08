#include <stdio.h>
#include "debug.h"

void debug(const DBusError *error) {
	if (error)
		fprintf(stderr, "%s: %s\n", error->name, error->message);
}
