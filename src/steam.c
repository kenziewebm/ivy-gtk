/* this code should be illegal */

#if defined(_WIN32)
#include <windows.h>
#endif

#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>

int get_steam_path_win(char* out, int max_len) {
	HKEY hkey;
	char installPath[MAX_PATH];
	DWORD bufsize = sizeof(installPath);
	DWORD type;

	LONG result = RegOpenKeyExA(
		HKEY_LOCAL_MACHINE,
		"SOFTWARE\\Valve\\Steam",
		0,
		KEY_READ | KEY_WOW64_64KEY,
		&hkey
	);

	if (result != ERROR_SUCCESS) {
		g_print("[steam] regopenkeyex failed: %ld\n", result);
		return 1;
	}

	result = RegQueryValueExA(
		hkey,
		"InstallPath",
		NULL,
		&type,
		(LPBYTE)installPath,
		&bufsize
	);

	RegCloseKey(hkey);

	if (result != ERROR_SUCCESS || type != REG_SZ) {
		g_print("[steam] failed to get key: %ld\n", result);
		return 1;
	}

	installPath[sizeof(installPath) - 1] = '\0';

	g_print("[steam] steam at: %s\n", installPath);

	snprintf(out, max_len, "%s\\steamapps\\libraryfolders.vdf", installPath);
}

int get_deadlock_path(char* library_path, char* out, int max_len) {
	g_print("[steam] %s\n", library_path);
	FILE* fd = fopen(library_path, "r");
	char current_folder[256];
	char line[256];
	if (fd != NULL) {
		while (fgets(line, sizeof(line), fd)) {
			char* pos = strstr(line, "path");
			if (pos) {
				int i = 0;
				char* tok = strtok(line, "\"");
				while (tok != 0) {
					if (i == 3) {
					       strcpy(current_folder, tok);
					       g_print("[library] %s\n", current_folder);
					}
					tok = strtok(0, "\"");
					i++;
				}
			}
			char* posi = strstr(line, "\"1422450\"");
			if (posi) {
				int i = 0;
				char* tok = strtok(line, "\"");
				while (tok != 0) {
					if (i == 2) {
						sprintf(out, "%s\\steamapps\\common\\Deadlock", current_folder);
						g_print("[library] deadlock at %s\n", out);
						return 0;
					}
					tok = strtok(0, "\"");
					i++;
				}
			}
		}
		fclose(fd);
		return 1;
	} else {
		g_print("[steam] fopen fail?\n");
		return 1;
	}
}

#if defined(_WIN32)
int get_steam_path(char* out, int max_len) { return get_steam_path_win(out, max_len); }
#elif defined(__APPLE__)
int get_steam_path(char* out, int max_len) { return get_steam_path_apple(out, max_len); }
#else
int get_steam_path(char* out, int max_len) { return get_steam_path_unix(out, max_len); }
#endif

