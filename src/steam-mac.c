#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "steam.h"

static char *resolve_library_path(const char *library_path, const char *value) {
	/* Native Steam already stores a macOS path */
	if (!g_ascii_isalpha(value[0]) || value[1] != ':') return g_strdup(value);
	char *folder = g_strcompress(value);
	g_strdelimit(folder, "\\", '/');
	if (folder[2] != '/') {
		g_free(folder);
		return NULL;
	}
	/* Resolve Windows drives relative to the bottle containing Steam */
	const char *drive_c = strstr(library_path, "/drive_c/");
	if (!drive_c) {
		g_free(folder);
		return NULL;
	}
	char *bottle = g_strndup(library_path, drive_c - library_path);
	char drive[] = {g_ascii_tolower(folder[0]), ':', '\0'};
	char *path;
	if (drive[0] == 'c') path = g_build_filename(bottle, "drive_c", folder + 3, NULL);
	else path = g_build_filename(bottle, "dosdevices", drive, folder + 3, NULL);
	g_free(bottle);
	g_free(folder);
	return path;
}

int get_steam_path(char *out, int max_len) {
	/* try CrossOver, then native Steam */
	const char *paths[] = {
		"Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/libraryfolders.vdf",
		"Library/Application Support/Steam/steamapps/libraryfolders.vdf"
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
			char *path = resolve_library_path(library_path, value);
			folder[0] = '\0';
			if (path) g_strlcpy(folder, path, sizeof(folder));
			g_free(path);
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
