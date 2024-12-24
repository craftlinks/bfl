#include <stdint.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define NSV Nob_String_View
#define NSB Nob_String_Builder

/*
COLOR
*/

typedef struct {
    uint8_t r, g, b;
} Color;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RGBA32;

Color colors[11] = {
    {128, 128, 128},
    {255, 0, 0},    // Red
    {0, 0, 255},    // Blue
    {0, 180, 0},    // Green
    {255, 140, 0},  // Orange
    {147, 0, 211},  // Purple
    {0, 206, 209},  // Turquoise
    {255, 105, 180},// Pink
    {139, 69, 19},  // Brown
    {255, 215, 0},  // Yellow // Gray
    {0, 0 ,0}       // Black
};

typedef struct {
    NSV *items;
    int count;
    int capacity;
} Programs;

Color instruction_to_color(char i) {
    switch (i) {
        case 'o':
            return colors[0];
        case '<':
            return colors[1]; 
        case '>':
            return colors[2];
        case '{':
            return colors[3];
        case '}':
            return colors[4];
        case '|':
            return colors[5];
        case 'p':
            return colors[6];
        case 'm':
            return colors[7];
        case 'l':
            return colors[8];
        case 'r':
            return colors[9];
        default:
            return colors[10];
    }  
}

bool image_from_file(const char *file) {
    NSB buf = {0}; // Owns the memory
    if (!nob_read_entire_file(file, &buf)) return 1;
    nob_log(NOB_INFO, "Size of %s is %zu bytes", file, buf.count); // 
    
    NSV content = {
        .data = buf.items,
        .count = buf.count
    }; // Borrows the memory
    
    Programs programs = {0};
    size_t count = 0;
    for (; content.count > 0; ++count) {
        content = nob_sv_trim_left(content);
        content = nob_sv_trim_right(content);
        
        NSV program = nob_sv_chop_by_delim(&content, '\n');
        if (program.count != 256) {
            nob_log(NOB_INFO, "unexpected program size %zu, for program at line %zu", program.count, count);
            return true;
        } 
        
        nob_da_append(&programs, program);   
    }
    size_t p_size = programs.items[0].count;
    RGBA32 *pixels = malloc(programs.count * p_size * sizeof(RGBA32));
    
    nob_log(NOB_INFO, "writing image to pixels (h: %zu, w: %zu)", programs.count, p_size);
    
    for (size_t p = 0; p < (size_t)programs.count; ++p) {
        if (programs.items[p].count != p_size) {
                nob_log(NOB_ERROR, "Inconsistent program sizes: %zu != %zu", programs.items[p].count, p_size);
                return false;
            }
        for (size_t i = 0; i < p_size; ++i) {
            Color c = instruction_to_color(programs.items[p].data[i]);
            size_t index = p * p_size + i;
            pixels[index].r = c.r;
            pixels[index].g = c.g;
            pixels[index].b = c.b;
            pixels[index].a = 255;
        }
    }
    size_t len = strlen(file);
    char *im_file;
    if (len > 4 && strcmp(file + len - 4, ".txt") == 0) {
        im_file = malloc(len + 1); // +1 for null terminator
        strcpy(im_file, file);
        strcpy(&im_file[len-4], ".png");
    } else {
        im_file = malloc(len + 5);
        strcpy(im_file, file);
        strcpy(&im_file[len-4], ".png"); 
    }
    
    if(!stbi_write_png(im_file, p_size, programs.count, 4, pixels, p_size*sizeof(RGBA32))) return false;
    
    nob_log(NOB_INFO, "found %zu programs", programs.count);
    nob_log(NOB_INFO, "generated image file for %s", file);
    free(pixels);
    free(im_file);
    nob_da_free(programs);
    nob_da_free(buf);
    return true;
}

bool images_from_directory(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) {
        nob_log(NOB_ERROR, "Could not open directory %s", dir);
        return false;
    }

    struct dirent *ent;
    size_t dir_len = strlen(dir);

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type != DT_REG) continue;

        const char *name = ent->d_name;
        size_t name_len = strlen(name);

        if (name_len < 4 || strcmp(name + name_len - 4, ".txt") != 0) continue;

        char *full_path = malloc(dir_len + name_len + 2);
        sprintf(full_path, "%s/%s", dir, name);

        bool success = image_from_file(full_path);
        free(full_path);

        if (!success) {
            closedir(d);
            return false;
        }
    }

    closedir(d);
    nob_log(NOB_INFO, "processed images for directory %s", dir);
    return true;
}

char *cmd_value(int *argc,char ***argv) {
    if ((*argc) <= 0) {
        nob_log(NOB_ERROR, "No argument is provided for directory of file");
        return NULL;
    }
    return nob_shift(*argv, *argc);
}

int main(int argc, char **argv) {
    const char *program_name = nob_shift(argv, argc);
    (void)program_name;
    while (argc > 0) {
         const char *flag = nob_shift(argv, argc);
         if (strcmp(flag, "-d") == 0) {
            const char *dir = cmd_value(&argc, &argv);
            nob_log(NOB_INFO, "processing directory %s", dir);
            if(!images_from_directory(dir)) return 1; 
         }
         else if (strcmp(flag, "-f") == 0) {
            const char *file= cmd_value(&argc, &argv);
            nob_log(NOB_INFO, "processing file %s", file);
            if(!image_from_file(file)) return 1; 
         }
    }
}
