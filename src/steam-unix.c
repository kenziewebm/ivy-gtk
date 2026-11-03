#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "steam.h"

int get_steam_path(char *out, int max_len) {
	const char *paths[] = {
		".local/share/Steam/steamapps/libraryfolders.vdf",
		".steam/steam/steamapps/libraryfolders.vdf",
		".steam/root/steamapps/libraryfolders.vdf",
		".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/libraryfolders.vdf"
	};
	out[0] = '\0';
	for (size_t i = 0; i < G_N_ELEMENTS(paths); i++) {
		char *path = g_build_filename(g_get_home_dir(), paths[i], NULL);
		if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
			g_strlcpy(out, path, max_len);
			g_free(path);
			return 0;
		}
		g_free(path);
	}
	g_print("[steam] couldn't find libraryfolders.vdf\n");
	return 1;
}

int get_deadlock_path(char *library_path, char *out, int max_len) {
	FILE *fd = fopen(library_path, "r");
	char folder[1024] = "";
	char line[2048];
	out[0] = '\0';
	if (!fd) return 1;
	while (fgets(line, sizeof(line), fd)) {
		char *key = strchr(line, '"');
		if (!key) continue;
		char *end = strchr(++key, '"');
		if (!end) continue;
		*end = '\0';
		if (!strcmp(key, "path")) {
			char *value = strchr(end + 1, '"');
			if (!value) continue;
			end = strchr(++value, '"');
			if (!end) continue;
			*end = '\0';
			g_strlcpy(folder, value, sizeof(folder));
		}
		if (!strcmp(key, "1422450") && folder[0]) {
			char *path = g_build_filename(folder, "steamapps", "common", "Deadlock", NULL);
			g_strlcpy(out, path, max_len);
			g_free(path);
			fclose(fd);
			return 0;
		}
	}
	fclose(fd);
	g_print("[steam] couldn't find Deadlock in Steam libraries\n");
	return 1;
}
