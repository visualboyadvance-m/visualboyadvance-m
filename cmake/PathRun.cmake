function(make_path_run_wrapper cmd target)
    get_filename_component(cmd_resolved "${cmd}" REALPATH)
    get_filename_component(base_name "${cmd_resolved}" NAME)
    get_filename_component(dir_name  "${cmd_resolved}" DIRECTORY)

    set(source "${target}.c")

    file(WRITE "${source}"
"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUF_SZ 4096

#ifdef _WIN32
    #include <process.h>
    #define PATH_SEP ';'
    #define setenv(var, val, dummy) _putenv_s(var, val)
    #define execvp my_execvp
#else
    #include <unistd.h>
    #define PATH_SEP ':'
#endif

char* dir_name  = \"${dir_name}\";
char* base_name = \"${base_name}\";

int main(int argc, char** argv) {
    size_t dir_len  = strlen(dir_name);
    char* path      = getenv(\"PATH\");
    size_t path_len = strlen(path);
    char* new_path  = malloc(dir_len + path_len + 2);
    char** new_argv = malloc(sizeof(char*) * argc);
    char** p;
    char buf[BUF_SZ];

    strcpy(new_path, dir_name);
    new_path[dir_len] = PATH_SEP;
    strcpy(new_path + dir_len + 1, path);

    setenv(\"PATH\", new_path, 1);

    free(new_path);

    p = new_argv;
    *(p++) = base_name;
    while (*(++argv)) *(p++) = *argv;
    *p = NULL;

    execvp(base_name, new_argv);

    // this is only reached if exec failed
    snprintf(buf, BUF_SZ, \"%s: exec failed\", argv[0]);
    perror(buf);

    return EXIT_FAILURE;
}

#ifdef _WIN32
int my_execvp(char* cmd, char** argv) {
    int ret = _spawnvp(_P_WAIT, cmd, argv);
    if (ret == -1) return ret;
    exit(ret);
}
#endif
")

    include(HostCompile)
    host_compile("${source}" "${target}")
endfunction()
