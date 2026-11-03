#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "steam.h"

int get_steam_path(char *out, int max_len) {
	HKEY key;
	char install_path[MAX_PATH];
	DWORD size = sizeof(install_path);
	DWORD type;
	out[0] = '\0';

	LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", 0,
		KEY_READ | KEY_WOW64_64KEY, &key);
	if (result != ERROR_SUCCESS) {
		g_print("[steam] couldn't open Steam registry key: %ld\n", result);
		return 1;
	}
	result = RegQueryValueExA(key, "InstallPath", NULL, &type, (LPBYTE)install_path, &size);
	RegCloseKey(key);
	if (result != ERROR_SUCCESS || type != REG_SZ) {
		g_print("[steam] couldn't read InstallPath: %ld\n", result);
		return 1;
	}
	install_path[sizeof(install_path) - 1] = '\0';
	char *path = g_build_filename(install_path, "steamapps", "libraryfolders.vdf", NULL);
	gboolean found = g_file_test(path, G_FILE_TEST_IS_REGULAR);
	if (found) g_strlcpy(out, path, max_len);
	else g_print("[steam] couldn't find libraryfolders.vdf\n");
	g_free(path);
	return found ? 0 : 1;
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
			char *decoded = g_strcompress(value);
			g_strlcpy(folder, decoded, sizeof(folder));
			g_free(decoded);
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
