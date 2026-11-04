#include <gio/gio.h>
#include <string.h>

#include "gameinfo.h"

typedef struct {
	const char *start, *end;
	char *text;
	char brace;
} Token;

typedef struct {
	const char *start, *end;
	int kind;
} SearchPath;

static const char *keys[] = {"Game", "Mod", "Write", "Game", "Mod", "Write", "Game"};
static const char *values[] = {"citadel/addons", "citadel", "citadel", "citadel", "core", "core", "core"};

/* Keep offsets into the original file so comments and unrelated settings survive. */
static gboolean next_token(const char **cursor, Token *token) {
	const char *p = *cursor;
	memset(token, 0, sizeof(*token));
	for (;;) {
		while (g_ascii_isspace(*p)) p++;
		if (g_str_has_prefix(p, "//")) {
			while (*p && *p != '\n') p++;
		} else if (g_str_has_prefix(p, "/*")) {
			const char *end = strstr(p + 2, "*/");
			if (!end) return FALSE;
			p = end + 2;
		} else break;
	}
	token->start = p;
	if (*p == '{' || *p == '}') token->brace = *p++;
	else if (*p == '"') {
		const char *start = ++p;
		while (*p && *p != '"') {
			if (*p == '\\' && p[1]) p++;
			p++;
		}
		if (!*p) return FALSE;
		token->text = g_strndup(start, p - start);
		p++;
	} else if (*p) {
		const char *start = p;
		while (*p && !g_ascii_isspace(*p) && *p != '{' && *p != '}' &&
			!g_str_has_prefix(p, "//") && !g_str_has_prefix(p, "/*")) p++;
		token->text = g_strndup(start, p - start);
	}
	token->end = p;
	*cursor = p;
	return TRUE;
}

static int path_kind(const char *key, const char *value) {
	for (int i = 0; i < (int)G_N_ELEMENTS(keys); i++) {
		if (!g_ascii_strcasecmp(key, keys[i]) && !g_ascii_strcasecmp(value, values[i])) return i;
	}
	return -1;
}

static gboolean read_block(const char **cursor, int scope, int depth, GArray *paths, int *found) {
	if (depth > 64) return FALSE;
	for (;;) {
		Token key, value;
		if (!next_token(cursor, &key)) return FALSE;
		if (key.brace == '}') return depth > 0;
		if (!key.text) return !key.brace && depth == 0;
		if (!next_token(cursor, &value)) { g_free(key.text); return FALSE; }
		gboolean ok = TRUE;
		if (value.brace == '{') {
			int child = -1;
			if (scope == 0 && !g_ascii_strcasecmp(key.text, "GameInfo")) child = 1;
			if (scope == 1 && !g_ascii_strcasecmp(key.text, "FileSystem")) child = 2;
			if (scope == 2 && !g_ascii_strcasecmp(key.text, "SearchPaths")) {
				child = 3;
				(*found)++;
			}
			ok = scope != 3 && read_block(cursor, child, depth + 1, paths, found);
		} else if (!value.text) ok = FALSE;
		else if (scope == 3) {
			SearchPath path = {key.start, value.end, path_kind(key.text, value.text)};
			g_array_append_val(paths, path);
		}
		g_free(key.text);
		g_free(value.text);
		if (!ok) return FALSE;
	}
}

gboolean check_gameinfo(const char *path, GError **error) {
	char *contents = NULL;
	gsize length;
	if (!g_file_get_contents(path, &contents, &length, error)) return FALSE;
	const char *cursor = contents;
	if (g_str_has_prefix(cursor, "\xef\xbb\xbf")) cursor += 3;
	GArray *paths = g_array_new(FALSE, FALSE, sizeof(SearchPath));
	int found = 0;
	gboolean ok = !memchr(contents, '\0', length) && read_block(&cursor, 0, 0, paths, &found) && found == 1;
	int count = 0;
	gboolean ordered = TRUE;
	const char *anchor = NULL;
	for (guint i = 0; i < paths->len; i++) {
		SearchPath entry = g_array_index(paths, SearchPath, i);
		if (entry.kind < 0) continue;
		if (!anchor) anchor = entry.start;
		if (entry.kind != count++) ordered = FALSE;
	}
	if (!ok || !anchor) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			"Couldn't read GameInfo/FileSystem/SearchPaths in gameinfo.gi. The file hasn't been changed.");
		ok = FALSE;
	} else if (!ordered || count != (int)G_N_ELEMENTS(keys)) {
		const char *newline = strstr(contents, "\r\n") ? "\r\n" : "\n";
		GString *updated = g_string_new(NULL);
		cursor = contents;
		for (guint i = 0; i < paths->len; i++) {
			SearchPath entry = g_array_index(paths, SearchPath, i);
			if (entry.kind < 0) continue;
			g_string_append_len(updated, cursor, entry.start - cursor);
			if (entry.start == anchor) {
				for (guint j = 0; j < G_N_ELEMENTS(keys); j++) {
					if (j) g_string_append_printf(updated, "%s\t\t\t", newline);
					g_string_append_printf(updated, "%s\t\t\t\t%s", keys[j], values[j]);
				}
			}
			cursor = entry.end;
		}
		g_string_append(updated, cursor);
		/* Replace atomically; keep our own backup without touching another manager's .bak. */
		char *backup = g_strconcat(path, ".ivy.bak", NULL);
		GFile *file = g_file_new_for_path(backup);
		GError *backup_error = NULL;
		GFileOutputStream *stream = g_file_create(file, G_FILE_CREATE_NONE, NULL, &backup_error);
		if (stream) {
			ok = g_output_stream_write_all(G_OUTPUT_STREAM(stream), contents, length, NULL, NULL, error);
			if (ok) ok = g_output_stream_close(G_OUTPUT_STREAM(stream), NULL, error);
			g_object_unref(stream);
			if (!ok) g_file_delete(file, NULL, NULL);
		} else if (g_error_matches(backup_error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
			ok = TRUE;
		} else {
			g_propagate_error(error, backup_error);
			backup_error = NULL;
			ok = FALSE;
		}
		g_clear_error(&backup_error);
		if (ok) ok = g_file_set_contents_full(path, updated->str, updated->len, G_FILE_SET_CONTENTS_CONSISTENT, 0644, error);
		g_object_unref(file);
		g_free(backup);
		g_string_free(updated, TRUE);
	}
	g_array_unref(paths);
	g_free(contents);
	return ok;
}
