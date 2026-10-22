/* this code should be illegal */

#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>

int get_steam_path(char* out, int max_len) {
    /* try crossover, then native steam */
    char* homedir = getenv("HOME");
    char steamdir[max_len];
    sprintf(steamdir, "%s/Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam", homedir);
    g_print("trying %s\n", steamdir);
    FILE* fd = fopen(steamdir, "r");
    if (!fd) g_print("nope, trying native steam\n");
    else {
        snprintf(out, max_len, "%s/steamapps/libraryfolders.vdf", steamdir);
        fclose(fd);
        return 0;
    }
    fclose(fd);
    sprintf(steamdir, "%s/Library/Application Support/Steam", homedir);
    g_print("trying %s\n", steamdir);
    fd = fopen(steamdir, "r");
    if (!fd) {
        g_print("i give up\n");
        return 1;
    } else {
        snprintf(out, max_len, "%s/steamapps/libraryfolders.vdf", steamdir);
	fclose(fd);
        return 0;
    }
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
						snprintf(out, max_len, "%s/steamapps/common/Deadlock", current_folder);
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
