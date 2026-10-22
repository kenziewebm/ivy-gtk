
/* THIS SHIT IS NOT PRODUCTION SAFE */

#include <gtk/gtk.h>
#include <stdio.h>

char* sanitize_html(const char *html) {
	GError *error = NULL;
	char *text = g_strdup(html);

	const char *break_patterns[] = {
		"(?i)<br\\s*/?>",
		"(?i)</p\\s*>",
		"(?i)</div\\s*>",
		"(?i)</li\\s*>"
		};

	for (int i = 0; i < G_N_ELEMENTS(break_patterns); i++) {
		GRegex *regex = g_regex_new(break_patterns[i], 0, 0, &error);
		if (!regex) {
			g_print("[sanitizer] regex error: %s\n", error->message);
			g_clear_error(&error);
			continue;
		}
	
		char *next = g_regex_replace(regex, text, -1, 0, "\n", 0, &error);
		g_regex_unref(regex);
	
		if (next) {
			g_free(text);
			text = next;
		} else {
			g_print("[sanitizer] replacement error: %s\n", error->message);
			g_clear_error(&error);
		}
	}
	
	GRegex *tags = g_regex_new("<[^>]*>", 0, 0, &error);
	if (tags) {
		char* next = g_regex_replace(tags, text, -1, 0, "", 0, &error);
		g_regex_unref(tags);
	
		if (next) {
			g_free(text);
			text = next;
		}
	}
	g_clear_error(&error);
	
	const struct { const char *from, *to; } entities[] = {
		{ "&nbsp;", " " },
		{ "&amp;", "&" },
		{ "&gt;", ">" },
		{ "&quot;", "\"" }
	};
	
	for (int i = 0; i < G_N_ELEMENTS(entities); i++) {
		char *next = g_strdup(text);
		char *match;
	
		while ((match = strstr(next, entities[i].from)) != NULL) {
			int before = (int)(match - next);
			char *replaced = g_strdup_printf(
					"%.*s%s%s",
					(int)before, next,
					entities[i].to,
					match + strlen(entities[i].from));
			g_free(next);
			next = replaced;
		}
	
		g_free(text);
		text = next;
	}
	return text;
}


