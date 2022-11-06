#ifndef GIFMETADATA_GIF_H
#define GIFMETADATA_GIF_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// extension callbacks

enum extension_type {
	plain_text,
	application,
	application_subblock,
	comment
};
struct extension_info {
    enum extension_type type;
    char *buffer;
    size_t buffer_len;
};

// state callbacks

enum file_read_state {
	header,
	logical_screen_descriptor,
	global_color_table,
	control_extension,
	image_descriptor,
	local_color_table,
	image_data,
	extension,
	known_extension,
	unknown_extension,
	trailer,
	searching
};

// error
enum read_gif_file_status {
    GIF_FILE_SUCCESS,
    GIF_FILE_INVALID_SIG
};

enum read_gif_file_status read_gif_file(FILE *file, void (*extension_cb)(struct extension_info*), void (*state_cb)(enum file_read_state),  int verbose_flag, int dev_flag);

#endif