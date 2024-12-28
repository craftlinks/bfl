#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

// Folders must end with forward slash /
#define BUILD_FOLDER "build/"
#define SRC_FOLDER "./"

// TODO: redefine for your platform
#define builder_cc(cmd) \
    cmd_append(cmd, "cc")
#define builder_flags(cmd) \
    cmd_append(cmd, "-Wall", "-Wextra", "-Wswitch-enum", "-ggdb", "-I." )
#define builder_output(cmd, output_path) \
    cmd_append(cmd, "-o", output_path)
#define builder_inputs(cmd, ...) \
    cmd_append(cmd, __VA_ARGS__)
#define builder_libs(cmd) \
    cmd_append(cmd, "-lm")

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Cmd cmd = {0};
    const char *program_name = shift(argv, argc);

    if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    builder_cc(&cmd);
    builder_flags(&cmd);
    builder_inputs(&cmd, SRC_FOLDER"main.c");
    builder_output(&cmd, BUILD_FOLDER"detect_cycles");
    builder_libs(&cmd);
    if (!cmd_run_sync_and_reset(&cmd)) return 1;

    if (argc > 0) {
        const char *command_name = shift(argv, argc);
        if (strcmp(command_name, "run") == 0) {
            cmd_append(&cmd, BUILD_FOLDER "detect_cycles");
            da_append_many(&cmd, argv, argc);
            if (!cmd_run_sync_and_reset(&cmd)) return 1;
        } else {
            nob_log(ERROR, "Unknown command %s", command_name);
            return 1;
        }
    }

    return 0;
}
