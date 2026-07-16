#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define VERSION "0.1.0"

#define IMG_NAME_MAX 64

#define error(fmt, ...) ( \
    fprintf(stderr, "pacc: "), \
    fprintf(stderr, fmt, __VA_ARGS__), \
    fputc('\n', stderr), \
    fprintf(stderr, "type \'pacc --help\' or \'pacc -h\' for more information\n"), \
    exit(1) \
)

#define log(fmt, ...) ( \
    fprintf(stdout, "pacc: "), \
    fprintf(stdout, fmt, __VA_ARGS__), \
    fputc('\n', stdout) \
)

#define ROW_MAX 16
#define COL_MAX 16

static struct {
    int exist;
    char name[IMG_NAME_MAX];
} files[ROW_MAX][COL_MAX];
static int row_len = 0;
static int col_len = 0;

#define streq(a, b) (strcmp(a, b) == 0)

#define error_reqval(param) error("%s requires a value", param)

int main(int argc, char **argv)
{
    int space = 2;
    const char *output_name = "sheet.png";
    const char *atlas_name  = "atlas.json";
    int minify = 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (streq(arg, "--version") || streq(arg, "-v")) {
            printf("pacc %s\n", VERSION);
            return 0;
        }
        if (streq(arg, "--help") || streq(arg, "-h")) {
            printf("Usage: pacc [OPTIONS] < image-files-names\n"
                   "pacc combines multiple images into a single sprite sheet,\n"
                   "plus a JSON atlas describing each sprite's position.\n"
                   "\n"
                   "Reads image file paths from stdin. Each line becomes one row in the sheet;\n"
                   "multiple images on the same line are placed side-by-side in that row.\n"
                   "After each newline, a new row begins.\n"
                   "\n"
                   "Options:\n"
                   "  -h, --help          Show this help message and exit\n"
                   "  -v, --version       Show version and exit\n"
                   "  --space INTEGER     Pixels of padding around each image (default: 2)\n"
                   "  --output FILE       Output sheet file name (default: sheet.png)\n"
                   "  --atlas FILE        Output atlas file name (default: atlas.json)\n"
                   "  --minify            Disable pretty formatting in the atlas file\n"
                   "\n"
                   "Examples:\n"
                   "  pacc --space 8 --output my_sheet.png <<EOF\n"
                   "  image1.png image2.png\n"
                   "  image3.png\n"
                   "  image4.png\n"
                   "  EOF\n"
                   "\n"
                   "  echo \"image1.png image2.png\n"
                   "  image3.png\n"
                   "  image4.png\" > my_sprites.txt\n"
                   "\n"
                   "  cat my_sprites.txt | pacc --space 4 --minify\n"
            );
            return 0;
        }
        if (streq(arg, "--space")) {
            if (++i >= argc) {
                error_reqval("--space");
            }
            space = atoi(argv[i]);
        }
        else
        if (streq(arg, "--output")) {
            if (++i >= argc) {
                error_reqval("--output");
            }
            output_name = argv[i];
        }
        else
        if (streq(arg, "--atlas")) {
            if (++i >= argc) {
                error_reqval("--atlas");
            }
            atlas_name = argv[i];
        }
        else
        if (streq(arg, "--minify")) {
            minify = 1;
        }
        else {
            error("invalid argument: \'%s\'", arg);
        }
    }

    int c;
    char name[IMG_NAME_MAX];
    char *name_p = name;
    int w, h, ch;
    struct {
        int x, y;
        int w, h;
    } canvas = {0};
    canvas.x = space;
    canvas.h = space;

    int run = 1;
    while (run) {
        c = getc(stdin);
        if (c == EOF) {
            run = 0;
        }
        if (isspace(c) || c == EOF) {
            if (name != name_p) {
                *name_p = '\0';
                if (!stbi_info(name, &w, &h, &ch)) {
                    error("there is no file named \'%s\'", name);
                }
                if (col_len >= COL_MAX) {
                    error("exceeded columns: %d", COL_MAX);
                }
                if (row_len >= ROW_MAX) {
                    error("exceeded rows: %d", ROW_MAX);
                }
                memcpy(files[row_len][col_len].name, name, name_p - name + 1);
                files[row_len][col_len].exist = 1;
                col_len++;
                name_p = name;

                if (canvas.y < h) {
                    canvas.y = h;
                }
                canvas.x += w + space;
            }
            if (c == '\n' || c == EOF) {
                if (col_len > 0) {
                    if (canvas.w < canvas.x) {
                        canvas.w = canvas.x;
                    }
                    canvas.x = space;
                    canvas.h += canvas.y + space;
                    canvas.y = 0;
                    col_len = 0;
                    row_len++;
                }
            }
        } else {
            if (name_p - name >= sizeof(name) - 1) {
                error("maximum amount of file name chars is %d", IMG_NAME_MAX);
            }
            *name_p++ = c;
        }
    }

    log("opening canvas in %dx%d...", canvas.w, canvas.h);
    uint8_t *canvas_data = calloc(canvas.w * canvas.h, 4);
    if (!canvas_data) {
        error("out of memory", "");
    }

    FILE *atlas = fopen(atlas_name, "w");
    if (!atlas) {
        error("failed to open \'%s\'", atlas_name);
    }
    if (minify) {
        fprintf(atlas, "{\"space\":%d,\"output\":\"%s\",\"sprites\":{", space, output_name);
    } else {
        fprintf(atlas, "{\n  \"space\": %d,\n", space);
        fprintf(atlas, "  \"output\": \"%s\",\n", output_name);
        fputs("  \"sprites\": {\n", atlas);
    }

    canvas.y = space;
    for (int row = 0; row < row_len; row++) {
        canvas.x = space;
        int max_h = 0;
        for (int col = 0; col < COL_MAX; col++) {
            if (!files[row][col].exist)
                break;

            uint8_t *img = stbi_load(files[row][col].name, &w, &h, &ch, 4);
            if (!img) {
                error("failed to load \'%s\'", files[row][col].name);
            }
            if (minify) {
                fprintf(atlas,
                        "\"%s\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
                        files[row][col].name, canvas.x, canvas.y, w, h);

                if (row < row_len - 1 || (col < COL_MAX - 1 && files[row][col + 1].exist)) {
                    fputc(',', atlas);
                }
            } else {
                fprintf(atlas, "    \"%s\": {\n", files[row][col].name);
                fprintf(atlas, "      \"x\": %d,\n", canvas.x);
                fprintf(atlas, "      \"y\": %d,\n", canvas.y);
                fprintf(atlas, "      \"w\": %d,\n", w);
                fprintf(atlas, "      \"h\": %d\n", h);
                fputs("    }", atlas);
                if (row < row_len - 1 || (col < COL_MAX - 1 && files[row][col + 1].exist)) {
                    fputs(",\n", atlas);
                }
            }

            if (max_h < h)
                max_h = h;

            for (int oy = 0; oy < h; oy++) {
                int target = canvas.y + oy;
                int dest_offset = (target * canvas.w + canvas.x) * 4;
                int src_offset = oy * w * 4;
                memcpy(canvas_data + dest_offset, img + src_offset, w * 4);
            }
            stbi_image_free(img);

            canvas.x += w + space;
        }
        canvas.y += max_h + space;
    }

    if (minify)
        fputs("}}", atlas);
    else
        fputs("\n  }\n}", atlas);

    if (!stbi_write_png(output_name, canvas.w, canvas.h, 4, canvas_data, canvas.w * 4)) {
        error("failed to write to \'%s\'", output_name);
    }
    log("%s: saved.", output_name);
    fclose(atlas);
    log("%s: saved.", atlas_name);
    free(canvas_data);
    return 0;
}
