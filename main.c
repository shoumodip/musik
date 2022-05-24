#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#define SV_IMPLEMENTATION
#include "sv.h"

#define WIDTH 800
#define HEIGHT 600

#define PATH_BUFFER_CAP 1024
char path_buffer[PATH_BUFFER_CAP];
size_t path_buffer_size;

void path_buffer_push(SV sv)
{
    assert(path_buffer_size + sv.size < PATH_BUFFER_CAP);
    memcpy(path_buffer + path_buffer_size, sv.data, sv.size);
    path_buffer_size += sv.size;
}

void path_buffer_push_cstr(const char *data)
{
    path_buffer_push(sv_cstr(data));
}

void path_buffer_finish(void)
{
    path_buffer[path_buffer_size++] = '\0';
}

void path_buffer_rewind(void)
{
    path_buffer_size = 0;
}

char *read_file(const char *file_path, size_t *size)
{
    const int fd = open(file_path, O_RDONLY);
    if (fd < 0) goto error;

    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0) goto error;

    char *data = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) goto error;
    close(fd);

    *size = statbuf.st_size;
    return data;

error:
    fprintf(stderr, "Error: could not read file '%s': %s\n", file_path, strerror(errno));
    exit(1);
}

void scc(int code)
{
    if (code < 0) {
        fprintf(stderr, "[SDL Error] %s\n", SDL_GetError());
        exit(1);
    }
}

void *scp(void *ptr)
{
    if (!ptr) {
        fprintf(stderr, "[SDL Error] %s\n", SDL_GetError());
        exit(1);
    }
    return ptr;
}

void mcc(int code)
{
    if (code < 0) {
        fprintf(stderr, "[Mixer Error] %s\n", Mix_GetError());
        exit(1);
    }
}

void *mcp(void *ptr)
{
    if (!ptr) {
        fprintf(stderr, "[Mixer Error] %s\n", Mix_GetError());
        exit(1);
    }
    return ptr;
}

#define MUSIC_LIST_CAP 1024
Mix_Music *music_list[MUSIC_LIST_CAP];
size_t music_list_count;
size_t music_list_current;
bool music_list_updated;

void music_list_push(Mix_Music *music)
{
    assert(music_list_count < MUSIC_LIST_CAP);
    music_list[music_list_count++] = music;
}

void music_list_next(void)
{
    music_list_updated = false;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: smm PLAYLIST\n");
        fprintf(stderr, "Error: insufficient arguments\n");
        exit(1);
    }

    const SV name = sv_cstr(argv[1]);
    const SV home = sv_cstr(getenv("HOME"));

    path_buffer_push(home);
    path_buffer_push_cstr("/.config/smm.conf");
    path_buffer_finish();

    size_t size;
    char *data = read_file(path_buffer, &size);

    scc(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));
    SDL_Window *window = scp(SDL_CreateWindow("Simple Music Manager", 0, 0, WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE));
    SDL_Renderer *renderer = scp(SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));
    mcc(SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT));

    mcc((Mix_Init(MIX_INIT_MP3) == MIX_INIT_MP3) - 1);
    mcc(Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 640));
    Mix_HookMusicFinished(music_list_next);

    bool writing = false;
    SV config = sv(data, size);
    while (config.size) {
        SV line = sv_trim(sv_split(&config, '\n'), ' ');
        if (line.size == 0) {
            continue;
        }

        if (sv_starts_with(line, SVStatic("#"))) {
            if (writing) {
                break;
            }

            sv_advance(&line, 1);
            line = sv_trim(line, ' ');
            if (sv_eq(line, name)) {
                writing = true;
            }
        } else if (writing) {
            path_buffer_rewind();
            if (sv_starts_with(line, SVStatic("~"))) {
                sv_advance(&line, 1);
                path_buffer_push(home);
            }

            path_buffer_push(line);
            path_buffer_finish();
            music_list_push(mcp(Mix_LoadMUS(path_buffer)));
        }
    }

    if (!writing) {
        fprintf(stderr, "Error: no playlist named '" SVFmt "' found\n", SVArg(name));
        exit(1);
    }

    munmap(data, size);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case 'q':
                    running = false;
                    break;

                case ' ':
                    if (Mix_PausedMusic()) {
                        Mix_ResumeMusic();
                    } else {
                        Mix_PauseMusic();
                    }
                    break;
                }
                break;
            }
        }

        if (!music_list_updated) {
            Mix_PlayMusic(music_list[music_list_current++], 1);
            music_list_updated = true;
        }
    }

    Mix_HaltMusic();
    for (size_t i = 0; i < music_list_count; ++i) {
        Mix_FreeMusic(music_list[i]);
    }
    Mix_CloseAudio();
    Mix_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
