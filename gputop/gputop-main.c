/*
 * GPU Top
 *
 * Copyright (C) 2013,2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


static void
usage(void)
{
    printf ("Usage: gputop [options] <program> [program args...]\n"
	    "\n"
	    "     --libgl=<libgl_filename>      Explicitly specify the real libGL library to intercept\n"
	    "     --libegl=<libegl_filename>    Explicitly specify the real libEGL library to intercept\n"
	    " -h, --help                        Display this help\n\n");
    exit(1);
}

static void
resolve_lib_path_for_env(const char *lib, const char *sym_name, const char *env)
{
    void *lib_handle;
    void *sym;
    Dl_info sym_info;

    lib_handle = dlopen(lib, RTLD_LAZY);
    if (!lib_handle) {
        fprintf(stderr, "gputop: Failed to dlopen \"%s\" while trying to resolve a default library path: %s\n",
                lib, dlerror());
        exit(1);
    }

    sym = dlsym(lib_handle, sym_name);
    if (!sym) {
        fprintf(stderr, "gputop: Failed to lookup \"%s\" symbol while trying to resolve a default %s path: %s\n",
                sym_name, lib, dlerror());
        exit(1);
    }

    if (dladdr(sym, &sym_info) == 0) {
        fprintf(stderr, "gputop: Failed to map %s symbol to a filename while trying to resolve a default %s path: %s\n",
                sym_name, lib, dlerror());
        exit(1);
    }

    setenv(env, sym_info.dli_fname, true);

    if (dlclose(lib_handle) != 0) {
        fprintf(stderr, "gputop: Failed to close %s handle after resolving default library path: %s\n",
                lib, dlerror());
        exit(1);
    }
}

int
main (int argc, char **argv)
{
    int opt;

#define LIB_GL_OPT  (CHAR_MAX + 1)
#define LIB_EGL_OPT (CHAR_MAX + 2)

    /* The initial '+' means that getopt will stop looking for
     * options after the first non-option argument. */
    const char *short_options="+h";
    const struct option long_options[] = {
	{"help",   no_argument,	      0, 'h'},
	{"libgl",  optional_argument, 0, LIB_GL_OPT},
	{"libegl", optional_argument, 0, LIB_EGL_OPT},
	{0, 0, 0, 0}
    };
    const char *prev_ld_library_path;
    char *ld_library_path;
    int err;
    int i;


    while ((opt = getopt_long(argc, argv, short_options, long_options, NULL))
	   != -1)
    {
	switch (opt) {
	    case 'h':
		usage();
		return 0;
	    case LIB_GL_OPT:
		setenv("GPUTOP_GL_LIBRARY", optarg, true);
		break;
	    case LIB_EGL_OPT:
		setenv("GPUTOP_EGL_LIBRARY", optarg, true);
		break;
	    default:
		fprintf (stderr, "Internal error: "
			 "unexpected getopt value: %d\n", opt);
		exit (1);
	}
    }

    if (optind >= argc) {
	fprintf(stderr, "Error: No program name provided\n");
	usage();
    }

    if (!getenv("GPUTOP_GL_LIBRARY"))
        resolve_lib_path_for_env("libGL.so.1", "glClear", "GPUTOP_GL_LIBRARY");

    if (!getenv("GPUTOP_EGL_LIBRARY"))
        resolve_lib_path_for_env("libEGL.so.1", "eglGetDisplay", "GPUTOP_EGL_LIBRARY");

    prev_ld_library_path = getenv("LD_LIBRARY_PATH");
    if (!prev_ld_library_path)
	prev_ld_library_path = "";

    asprintf(&ld_library_path, "%s:%s", GPUTOP_WRAPPER_DIR, prev_ld_library_path);
    setenv("LD_LIBRARY_PATH", ld_library_path, true);
    free(ld_library_path);

    execvp(argv[optind], &argv[optind]);
    err = errno;

    fprintf(stderr, "gputop: Failed to run GL application: \n\n"
	    "  ");
    for (i = optind; i < argc; i++)
	fprintf(stderr, "%s ", argv[i]);
    fprintf(stderr, "\n\n%s\n", strerror(err));

    return 0;
}