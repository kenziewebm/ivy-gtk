/* Build tool: wrap resized PNGs in Windows ICO and macOS ICNS containers. */
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

static void append_u16(GByteArray *bytes, guint16 value) {
    value = GUINT16_TO_LE(value);
    g_byte_array_append(bytes, (const guint8 *)&value, sizeof(value));
}

static void append_u32(GByteArray *bytes, guint32 value, gboolean big_endian) {
    value = big_endian ? GUINT32_TO_BE(value) : GUINT32_TO_LE(value);
    g_byte_array_append(bytes, (const guint8 *)&value, sizeof(value));
}

static GBytes *resize_png(GdkPixbuf *source, int size, GError **error) {
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(source, size, size, GDK_INTERP_HYPER);
    if (!scaled) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "Could not resize icon");
        return NULL;
    }
    gchar *buffer = NULL;
    gsize length = 0;
    gboolean saved = gdk_pixbuf_save_to_buffer(scaled, &buffer, &length, "png", error, NULL);
    g_object_unref(scaled);
    return saved ? g_bytes_new_take(buffer, length) : NULL;
}

static gboolean write_ico(GdkPixbuf *source, const char *path, GError **error) {
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    GByteArray *headers = g_byte_array_new();
    GByteArray *images = g_byte_array_new();
    append_u16(headers, 0);
    append_u16(headers, 1);
    append_u16(headers, G_N_ELEMENTS(sizes));
    gboolean success = FALSE;
    for (guint i = 0; i < G_N_ELEMENTS(sizes); i++) {
        GBytes *png = resize_png(source, sizes[i], error);
        if (!png) goto done;
        gsize length;
        const guint8 *data = g_bytes_get_data(png, &length);
        guint8 dimensions[] = {(guint8)sizes[i], (guint8)sizes[i], 0, 0};
        g_byte_array_append(headers, dimensions, sizeof(dimensions));
        append_u16(headers, 1);
        append_u16(headers, 32);
        append_u32(headers, length, FALSE);
        append_u32(headers, 6 + 16 * G_N_ELEMENTS(sizes) + images->len, FALSE);
        g_byte_array_append(images, data, length);
        g_bytes_unref(png);
    }
    g_byte_array_append(headers, images->data, images->len);
    success = g_file_set_contents(path, (const gchar *)headers->data, headers->len, error);
done:
    g_byte_array_unref(headers);
    g_byte_array_unref(images);
    return success;
}

static gboolean write_icns(GdkPixbuf *source, const char *path, GError **error) {
    const struct { const char *type; int size; } entries[] = {
        {"icp4", 16}, {"icp5", 32}, {"icp6", 64}, {"ic07", 128},
        {"ic08", 256}, {"ic09", 512}, {"ic10", 1024},
        {"ic11", 32}, {"ic12", 64}, {"ic13", 256}, {"ic14", 512}
    };
    GByteArray *bytes = g_byte_array_new();
    g_byte_array_append(bytes, (const guint8 *)"icns", 4);
    append_u32(bytes, 0, TRUE);
    gboolean success = FALSE;
    for (guint i = 0; i < G_N_ELEMENTS(entries); i++) {
        if (entries[i].size > gdk_pixbuf_get_width(source)) continue;
        GBytes *png = resize_png(source, entries[i].size, error);
        if (!png) goto done;
        gsize length;
        const guint8 *data = g_bytes_get_data(png, &length);
        g_byte_array_append(bytes, (const guint8 *)entries[i].type, 4);
        append_u32(bytes, length + 8, TRUE);
        g_byte_array_append(bytes, data, length);
        g_bytes_unref(png);
    }
    guint32 length = GUINT32_TO_BE(bytes->len);
    memcpy(bytes->data + 4, &length, sizeof(length));
    success = g_file_set_contents(path, (const gchar *)bytes->data, bytes->len, error);
done:
    g_byte_array_unref(bytes);
    return success;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        g_printerr("usage: %s source.png output.ico output.icns\n", argv[0]);
        return 1;
    }
    GError *error = NULL;
    GdkPixbuf *source = gdk_pixbuf_new_from_file(argv[1], &error);
    if (source && (gdk_pixbuf_get_width(source) != gdk_pixbuf_get_height(source) ||
                   gdk_pixbuf_get_width(source) < 256)) {
        g_set_error_literal(&error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
            "App icon must be square and at least 256x256 pixels");
    }
    gboolean success = source && !error && write_ico(source, argv[2], &error) &&
        write_icns(source, argv[3], &error);
    if (!success) g_printerr("Icon conversion failed: %s\n", error ? error->message : "unknown error");
    g_clear_error(&error);
    g_clear_object(&source);
    return success ? 0 : 1;
}
