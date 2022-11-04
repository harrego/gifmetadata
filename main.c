// MIT License
// 
// Copyright (c) 2022 Harry Stanton
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// gifmetadata
// version 0.0.1
//
// Harry Stanton <harry@harrystanton.com>
// https://github.com/harrego/gifmetadata
//
// designed to
//     1. be fast
//     2. be grep able
//     3. use little memory
//     4. be conservative with libraries
//
// understanding of how gifs work comes from
//     http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html
// and
//     https://www.w3.org/Graphics/GIF/spec-gif89a.txt

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

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

// lsd = local screen descriptor
enum lsd_state {
	width,
	height,
	packed,
	bg_color,
	pixel_aspect_ratio
};

enum extension_type {
	plain_text,
	application,
	application_subblock,
	comment
};

const char gif_sig[] = { 'G', 'I', 'F' };
const char gif_87a[] = { '8', '7', 'a'};
const char gif_89a[] = { '8', '9', 'a'};

const char arg_verbose[] = "verbose";
const char arg_dev[] = "dev";
const char arg_help[] = "help";

// custom strncpy implementation to avoid
// including string.h, be warned the
// less than/greater than functionality
// isn't implemented
char strncpy_(const char *s1, const char *s2, size_t n) {
	for (int x = 0; x < n; x++) {
		if (s1[x] != s2[x])
			return 1;
	}
	return 0;
}

void print_help() {
	printf("gifmetadata\n");
	printf("version 0.0.1\n\n");

	printf("Harry Stanton <harry@harrystanton.com>\n");
	printf("https://github.com/harrego/gifmetadata\n\n");
	
	printf("OVERVIEW:\n");
	printf("    GIFs contain 'comments' that were commonly used to attribute copyright\n");
	printf("    and attribution in the early days of the web. Since then, programs have\n");
	printf("    lost the ability to read and write this data.\n\n");
	
	printf("    gifmetadata reads and outputs this data.\n\n");
	
	printf("OUTPUT:\n");
	printf("    gifmetadata can read comments, application extensions and plain text\n");
	printf("    embedded within a GIF.\n\n");
	
	printf("    comments:                Text messages limited to 256 characters, primarily\n");
	printf("                             copyright and attribution messages. Prefixed with\n");
    printf("                             \"comment:\".\n\n");
    
	printf("    application extensions:  Custom extensions to GIFs that applications\n");
	printf("                             may use to add additional features to the GIF.\n");
	printf("                             For example Netscape 2.0 used them to add early\n");
    printf("                             animation looping. Application extensions contain\n");
    printf("                             a name and then 'sub-blocks' of binary data, this\n");
    printf("                             may ping your terminal. Application name prefixed\n");
    printf("                             with \"application:\", sub-blocks with: \"-\".\n\n");
    
    printf("    plain text:              A feature within the 89a specification to display\n");
    printf("                             plain text on-top of images that was never utilized.\n");
    printf("                             Prefixed with \"plain text:\".\n\n");
	
	printf("USAGE: gifmetadata [options] file\n\n");
	
	printf("OPTIONS:\n");
	
	printf("    -h / --help      Display help, options and program info\n");
	printf("    -v / --verbose   Display more data about the gif, e.g. width/height\n");
	printf("    -d / --dev       Display inner program workings intended for developers\n");
}

int main(int argc, char **argv) {

	// reading argv code
	//
	// cool note...apparently int is faster than
	// char even tho its a bigger size bcs
	// cpus are designed to work with ints
	// https://stackoverflow.com/questions/9521140/char-or-int-for-boolean-value-in-c
	int verbose_flag = 0;
	int dev_flag = 0;

	char *filename = NULL;
	for (int x = 1; x < argc; x++) {
		int y = 0;
		if (argv[x][0] == '-') {
			if (argv[x][1] == '-') {
				// calculate the length of the
				// long arg
				y = 2;
				while (argv[x][y] != 0)
					y++;
				int arg_len = y - 2 + 1;
				
				if (arg_len < 1)
					break;
				
				// copy the long arg into a buffer
				char *long_arg = malloc(sizeof(char) * arg_len);
				for (y = 2; y < arg_len + 2; y++)
					long_arg[y - 2] = argv[x][y];
				
				// check if buffer matches the known
				// buffers
				
				// dirty self strncmp... if i add more
				// args ill just make a function
				if (arg_len == sizeof(arg_verbose)) {
					if (strncpy_(long_arg, (const char*)&arg_verbose, arg_len) == 0) {
						verbose_flag = 1;
						free(long_arg);
						continue;
					}
				} else if (arg_len == sizeof(arg_dev)) {
					if (strncpy_(long_arg, (const char*)&arg_dev, arg_len) == 0) {
						dev_flag = 1;
						free(long_arg);
						continue;
					}
				} else if (arg_len == sizeof(arg_help)) {
					if (strncpy_(long_arg, (const char*)&arg_help, arg_len) == 0) {
						free(long_arg);
						if (filename != NULL)
							free(filename);
						print_help();
						return 1;
					}
				}
				
				printf("[error] unknown flag: %s\n", long_arg);
				if (filename != NULL)
					free(filename);
				free(long_arg);
				return 1;
			} else {
				y = 1;
				while (argv[x][y] != '\0') {
					switch (argv[x][y]) {
					case 'v':
						verbose_flag = 1;
						break;
					case 'd':
						dev_flag = 1;
						break;
					case 'h':
						if (filename != NULL)
							free(filename);
						print_help();
						return 1;
					default:
						printf("[error] unknown flag: %c\n", argv[x][y]);
						if (filename != NULL)
							free(filename);
						return 1;
					}
					y++;
				}
			}
		} else {
			if (filename != NULL) {
				printf("[error] specified more than one file, i can only read one\n");
				free(filename);
				return 1;
			}
			// calculate filename len
			y = 0;
			while (argv[x][y] != 0)
				y++;
			// allocate filename buffer
			int filename_len = y + 1;
			filename = malloc(sizeof(char) * filename_len);
			// copy filename buffer
			for (y = 0; y < filename_len; y++)
				filename[y] = argv[x][y];
		}
	}
	
	if (dev_flag) {
		printf("[dev] dev flag active\n");
		if (verbose_flag) {
			printf("[dev] verbose flag active\n");
		}
	}
	
	if (filename == NULL) {
		printf("[error] you never specified a file to open\n");
		return 1;
	}

	FILE *fileptr;
	long filelen;
	
	if (access(filename, F_OK) != 0) {
		printf("[error] file '%s' cannot be accessed\n", filename);
		free(filename);
		return 1;
	}
	
	fileptr = fopen(filename, "rb");
	fseek(fileptr, 0, SEEK_END);
	filelen = ftell(fileptr);
	rewind(fileptr);
	
	if (verbose_flag)
		printf("[verbose] opened file '%s'\n", filename);
	
	if (verbose_flag)
		printf("[verbose] file size: %ld bytes\n", filelen);
	
	// step 1: check file is a gif
	if (6 > filelen) {
		printf("[error] file does not appear to be a gif (too small)\n");
		fclose(fileptr);
		free(filename);
		return 1;
	}
	
	// program reads the file in 256 chunks, and then requests
	// more if it needs them
	
	enum file_read_state state = header;
	unsigned char buffer[256];
	
	int color_table_len = 0;
	
	// scratchpad is a buffer used for misc reading/writing
	// while traversing thru the gif. 256 is enough since
	// the size as given by gif blocks can only be represented
	// in an unsigned char.
	unsigned char *scratchpad = malloc(sizeof(unsigned char) * 256);
	// i is max index of written data in the buffer
	int scratchpad_i = 0;
	// len is the max length to write into the buffer according
	// to an unsigned char that prefixes data
	int scratchpad_len = 0;
	
	// local screen descriptor
	enum lsd_state local_lsd_state;
	enum extension_type local_extension_type;
	
	int bytes_to_read;
	while ((bytes_to_read = fread(buffer, sizeof(unsigned char), 256, fileptr)) > 0) {
		int i = 0;
		
		// safe to assume that magic numbers fit in
		// a full 256 chunk bcs its at the start of
		// a file
		if (state == header) {

			for (i = 0; i < sizeof(gif_sig); i++) {
				if (buffer[i] != gif_sig[i]) {
					printf("[error] file does not appear to be a gif (wrong sig)\n");
					fclose(fileptr);
					free(filename);
					return 1;
				}
			}
			char unsupported_version = 0;
			for (; i < 6; i++) {
				if (buffer[i] != gif_87a[i] && buffer[i] != gif_89a[i]) {
					unsupported_version = 1;
					break;
				}
			}
			if (verbose_flag) {
				if (buffer[3] == '8') {
					if (buffer[4] == '7') {
						printf("[verbose] gif is version 87a\n");
					} else if (buffer[4] == '9') {
						printf("[verbose] gif is version 89a\n");
					}
				}
			}
			if (unsupported_version) {
				printf("[warning] gif is an unsupported version: ");
				for (i = sizeof(gif_sig); i < 6; i++) {
					printf("%c", buffer[i]);
				}
				printf("\n");
			}
			state = logical_screen_descriptor;
			local_lsd_state = 0;
		}
		
		for (; i < bytes_to_read; i++) {
			switch (state) {
			case logical_screen_descriptor:
				
				switch (local_lsd_state) {
				case width:
				case height:
					scratchpad[scratchpad_i] = buffer[i];
					scratchpad_i++;
					if (scratchpad_i >= 2) {
						int result = scratchpad[0] + (scratchpad[1] << 8);
						if (local_lsd_state == width) {
							if (verbose_flag)
								printf("[verbose] canvas width: %d\n", result);
							scratchpad_i = 0;
							local_lsd_state = height;
						} else {
							if (verbose_flag)
								printf("[verbose] canvas height: %d\n", result);
							scratchpad_i = 0;
							local_lsd_state = packed;
						}
					}
					break;
				case packed: {
					int color_resolution = ((buffer[i] & 0b1000000) >> 4) + ((buffer[i] & 0b100000) >> 4) + ((buffer[i] & 0b10000) >> 4);
					color_table_len = 3 * pow(2, color_resolution+1);
					
					if (dev_flag)
						printf("[dev] color depth is: %d, len is: %d\n", color_resolution, color_table_len);
					if ((buffer[i] & 0b10000000) == 0b10000000) {
						// global color table
						if (dev_flag)
							printf("[dev] has a global color table\n");
						
						int color_table_size = (buffer[i] & 0b100) + (buffer[i] & 0b10) + (buffer[i] & 0b1);
						color_table_len = 3 * pow(2, color_table_size+1);
						if (dev_flag)
							printf("[dev] color table size: %d, len: %d\n", color_table_size, color_table_len);
						
						
						// use the scratchpad index as color table
						// index
						scratchpad_i = 0;
						state = global_color_table;
					} else {
						state = searching;
					}
					
					break;
					}
				default:
					break;
				}
				
				break;
			case global_color_table:
				if (color_table_len < scratchpad_i) {
					if (dev_flag)
						printf("[dev] finished the glboal color table\n");
					state = searching;
				}
				scratchpad_i++;
				break;
			case searching:
				switch (buffer[i]) {
				case 0x21:
					if (dev_flag)
						printf("[dev] found an extension\n");
					state = extension;
					break;
				case 0x2c:
					if (dev_flag)
						printf("[dev] found an image descriptor\n");
					state = image_descriptor;
					scratchpad_i = 0;
					scratchpad_len = 0;
					break;
				case 0x3b:
					if (dev_flag)
						printf("[dev] found the trailer\n");
					// if this were a real gif parser you would terminate here
					// but i'm speculating that at least one gif
					// has been made with comment data coming after
					// the trailer as a mistake or easter egg:)~~
					state = trailer;
					break;
				default:
					if (dev_flag)
						printf("[dev] unknown byte: (0x%x)...\n", buffer[i]);
					break;
				}
				break;
			case extension:
				scratchpad_i = 0;
				scratchpad_len = 0;
				switch (buffer[i]) {
					case 0x01:
						state = known_extension;
						local_extension_type = plain_text;
						if (dev_flag)
							printf("[dev] found a plain text extension\n");
						break;
					case 0xff:
						state = known_extension;
						local_extension_type = application;
						if (dev_flag)
							printf("[dev] found an application extension\n");
						break;
					case 0xfe:
						state = known_extension;
						local_extension_type = comment;
						if (dev_flag)
							printf("[dev] found a comment extension\n");
						break;
					default:
						scratchpad_i = 0;
						scratchpad_len = 0;
						state = unknown_extension;
						if (dev_flag)
							printf("[dev] found an unknown extension\n");
						break;
				}
				break;
			case unknown_extension:
				if (scratchpad_len == 0) {
					if (buffer[i] == 0) {
						state = searching;
						break;
					}
					scratchpad_len = buffer[i];
					scratchpad_i = 0;
				} else {
					if (buffer[i] == 0x0 && scratchpad_i >= scratchpad_len) {
						if (dev_flag)
							printf("[dev] reached the end of the unknown extension\n");
						state = searching;
					} else {
						scratchpad_i++;
					}	
				}
				break;
			case known_extension:
				// if the scratchpatch len is empty
				// then must be new block
				if (scratchpad_len == 0) {
					// if the new size of the block is
					// zero then terminate
					if (buffer[i] == 0) {
						state = searching;
						if (dev_flag)
							printf("[dev] new extension block was empty\n");
						break;
					}
					// else get ready for a new block
					scratchpad_len = buffer[i];
					scratchpad_i = 0;
					if (dev_flag)
						printf("[dev] new extension block len: %d\n", scratchpad_len);
				} else {
					if (scratchpad_i < scratchpad_len) {
						scratchpad[scratchpad_i] = buffer[i];
						scratchpad_i++;
					} else {
						// if length has been met...
						if (local_extension_type == application || local_extension_type == application_subblock) {
							scratchpad[scratchpad_i] = '\0';
							scratchpad_i = 0;
							scratchpad_len = buffer[i];
							
							if (local_extension_type == application) {
								printf("application: %s\n", scratchpad);
								local_extension_type = application_subblock;
							} else {
								printf("- %s\n", scratchpad);
							}
							
							if (scratchpad_len == 0) {
								state = searching;
								break;
							}
						} else {
							if (buffer[i] == 0) {
								// if null terminated,
								// terminate and await next block
								scratchpad[scratchpad_i] = '\0';
								if (local_extension_type == plain_text) {
									printf("plain text: %s\n", scratchpad);
								} else if (local_extension_type == comment) {
									printf("comment: %s\n", scratchpad);
								}
								state = searching;
							}
							// else await the null terminator and
							// do not overflow buffer
						}
					}
					
				}
				break;
			case image_descriptor:
			
				if (scratchpad_i >= 8) {
					if (dev_flag)
						printf("[dev] reached the end of an image descriptor, now parsing\n");
					if ((buffer[i] & 0b10000000) == 0b10000000) {
						// local color table
						scratchpad_i = 0;
						int local_color_table = (buffer[i] & 0b100) + (buffer[i] & 0b10) + (buffer[i] & 0b1);
						scratchpad_len = 3*pow(2,local_color_table+1);
						state = local_color_table;
						if (dev_flag)
							printf("[dev] image descriptor contains a local color table with length %d\n", scratchpad_len);
					} else {
						scratchpad_i = 0;
						scratchpad_len = 0;
						state = image_data;
						break;
					}
				} else {
					scratchpad_i++;
				}
				break;
			case local_color_table:
				if (scratchpad_i >= scratchpad_len) {
					scratchpad_i = 0;
					scratchpad_len = 0;
					state = image_data;
					if (dev_flag)
						printf("[dev] reached the end of the local color table\n");
				} else {
					scratchpad_i++;
				}
				break;
			case image_data:
				if (scratchpad_len == 0) {
					if (scratchpad_i == 1) {
						scratchpad_i = 0;
						scratchpad_len = buffer[i];
						if (dev_flag)
							printf("[dev] start of image data blocks, initial block size: %d\n", scratchpad_len);
						break;
					}
				} else {
					if (scratchpad_i >= scratchpad_len) {
						if (buffer[i] == 0x0) {
							if (dev_flag)
								printf("[dev] reached the end of image data blocks\n");
							state = searching;
							break;
						} else {
							scratchpad_i = 0;
							scratchpad_len = buffer[i];
							if (dev_flag)
								printf("[dev] read an image block, next block size: %d\n", scratchpad_len);
							break;
						}
					}
				}
				scratchpad_i++;
				break;
			default:
				break;
			}
		}
		
	}
	free(filename);
	free(scratchpad);
	
	if (dev_flag)
		printf("[dev] finished reading image\n");
		
	if (state != trailer)
		printf("[warning] file was incompatible and therefore gifmetadata may have missed some data, recommended that you view this file in a hex editor to get more information\n");
	
	fclose(fileptr);

	return 0;
}