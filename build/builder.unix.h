/**
 * @file builder.unix.h
 * @brief The Unix-specific backend implementation for the builder framework.
 *
 * This file provides the concrete definitions for functions, types, and macros
 * required by `builder.h` for Unix-like operating systems (e.g., Linux, macOS).
 * It implements process creation, command execution, file system checks, and
 * logging using POSIX and standard C library APIs. This file is not meant to be
 * included directly, but rather through the main `builder.h` header.
 */
#ifndef __BUILDER_UNIX_H__
#define __BUILDER_UNIX_H__

#pragma once

// This is not just a header file, this is a source file within the header.
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @brief Prints a formatted error message to stderr, prefixed with the current build context name if available.
 * @param msg The format string for the message.
 * @param ... The variable arguments for the format string.
 */
#define error(msg, ...)                                                                        \
  if (build_context)                                                                           \
    fprintf(stderr, "\033[31merr:\033[m %s: " msg "\n", build_context->name, ##__VA_ARGS__);  \
  else                                                                                         \
    fprintf(stderr, "\033[31merr:\033[m " msg "\n", ##__VA_ARGS__)

/**
 * @brief Prints a formatted warning message to stderr, prefixed with the current build context name if available.
 * @param msg The format string for the message.
 * @param ... The variable arguments for the format string.
 */
#define warn(msg, ...)                                                                         \
  if (build_context)                                                                           \
    fprintf(stderr, "\033[33mwarn:\033[m %s: " msg "\n", build_context->name, ##__VA_ARGS__); \
  else                                                                                         \
    fprintf(stderr, "\033[31merr:\033[m " msg "\n", ##__VA_ARGS__)

/**
 * @brief Prints a formatted informational message to stdout, prefixed with the current build context name if available.
 * @param msg The format string for the message.
 * @param ... The variable arguments for the format string.
 */
#define info(msg, ...)                                                                     \
  if (build_context)                                                                       \
    printf("\033[34minfo:\033[m %s: " msg "\n", build_context->name, ##__VA_ARGS__);  \
  else                                                                                     \
    printf("\033[34minfo:\033[m " msg "\n", ##__VA_ARGS__)

/**
 * @brief Creates a scope for running asynchronous processes and waiting for them to complete.
 * It creates a new process list, waits for all child processes in that list to finish,
 * and then frees the list.
 */
#define SyncGroup()                                                               \
  for (pid_list_t *pid_list = pid_list_create(); pid_list != NULL;                 \
    pid_list_wait_sync(pid_list), pid_list_free(pid_list), pid_list = NULL)

/**
 * @brief Creates a compound literal of type `build_context_t` on the stack.
 * @param ... The initializers for the `build_context_t` struct.
 * @note The BUILDER_CONTEXT_DATA type is defined in a separate header.
 */
#define BuildContextOf(...) (&(build_context_t){__VA_ARGS__})

/**
 * @brief Creates a build context scope. It pushes a new context, executes the code within its block,
 * and then pops the context, restoring the previous one.
 * @param ... The initializers for the new `build_context_t`.
 */
#define BuildContext(...)                                                                                    \
  for (                                                                                                      \
    build_context_t *old = build_context_push(BuildContextOf(__VA_ARGS__)), *latch = (typeof(latch))0x01; \
    latch != NULL && build_context_do_begin(old);                                                          \
    latch = NULL, build_context_pop(old)                                                                     \
  ) SyncGroup()

/**
 * @brief Runs a command synchronously and waits for it to complete.
 * @param ... A list of string arguments for the command, terminated by NULL.
 * @note Relies on the StringArrayN macro, which is defined elsewhere.
 */
#define $_sync(...)                                                                   \
    wait_pid_sync(                                                                    \
        run_command(StringArrayN(__VA_ARGS__)[0], StringArrayN(__VA_ARGS__)));

/**
 * @brief Runs a command asynchronously, adding its process ID to the current `SyncGroup`'s pid_list.
 * @param ... A list of string arguments for the command, terminated by NULL.
 * @note Relies on the StringArrayN macro, which is defined elsewhere.
 */
#define $(...)                                                                        \
    pid_list_add(pid_list, run_command(StringArrayN(__VA_ARGS__)[0],                 \
        StringArrayN(__VA_ARGS__)));

/**
 * @brief Defines the main function body, adding logic to automatically recompile the
 * build script if its source is newer than the executable.
 * @note This macro is intended to be used as: `int main main_impl`.
 * @note Relies on `builder_entrypoint` and the `arguments` global array, which are defined elsewhere.
 */
#define main_impl (int argc, char **argv) {                                         \
  static char *source_file = __FILE__;                                              \
  if (is_file_older(argv[0], source_file)) {                                        \
    char host_cc[PATH_MAX] = {0};                                                   \
    info("build script is newer than the current executable, "                      \
        "recompiling...");                                                          \
                                                                                    \
    /* Find a C compiler to recompile the script*/                                  \
    find_executable("cc", host_cc);                                                 \
    *host_cc == 0 && find_executable("clang", host_cc);                             \
    *host_cc == 0 && find_executable("gcc", host_cc);                               \
                                                                                    \
    if (*host_cc == 0) {                                                            \
      error("Failed to find host C compiler");                                      \
      return 1;                                                                     \
    }                                                                               \
                                                                                    \
    int code = $_sync(host_cc, "-o", argv[0], (char *)source_file, NULL);            \
                                                                                    \
    if (code != 0) {                                                                \
      error("compilation failed with code %d", code);                               \
                                                                                    \
      return 1;                                                                     \
    }                                                                               \
                                                                                    \
    info("re-running build script again...");                                       \
    info("--------------------------------");                                        \
                                                                                    \
    /* Replace our process with the re-compiled build script */                     \
    execv(argv[0], argv);                                                           \
  }                                                                                 \
  struct arguments_t *args = builder_parse_arguments(                             \
    argc,                                                                           \
    argv,                                                                           \
    arguments,                                                                      \
    sizeof(arguments) / sizeof(arguments[0])                                        \
  );                                                                                \
  builder_entrypoint(args);                                                         \
  builder_free_arguments(args);                                                     \
}

//////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Globals ///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/** @brief The name of the currently executing program. */
impl(static char *program_name = NULL);
/** @brief The current build mode (e.g., "debug", "release"). */
impl(static char *build_mode = NULL);

//////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Build context ////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/**
 * @brief Represents a named scope for build operations.
 * @note The BUILDER_CONTEXT_DATA type is defined in a separate header.
 */
typedef struct build_context_t {
  /** @brief The name of the context (e.g., "Compiling library"). */
  char *name,
  /** @brief The mode this context applies to (e.g., "debug"). If NULL, applies to all modes. */
       *mode;
  /** @brief User-defined data associated with the context. */
  BUILDER_CONTEXT_DATA data;
} build_context_t;

/** @brief The currently active build context. */
impl(static build_context_t *build_context);

/**
 * @brief Pushes a new build context onto the context stack.
 * @param context The new context to make active.
 * @return The previously active context, to be restored later.
 */
build_context_t *build_context_push(build_context_t *context) impl({
  build_context_t *old = build_context;

  if (!context->name) {
    error("Build context created without name");

    context->name = "<unnamed context>";
  }

  build_context = context;

  return old;
});

/**
 * @brief Checks if the current build context should be executed and prints an "entering" message.
 * @param old The previous build context, used to revert if the current one is skipped.
 * @return 1 if the context should proceed, 0 otherwise.
 */
int build_context_do_begin(build_context_t *old) impl({
  if (!build_context) {
    return 0;
  }

  if (build_context->mode && strcmp(build_context->mode, build_mode) != 0) {
    build_context = old;

    return 0;
  }

  info("entering \x1b[1m\x1b[34m%s\033[m...", build_context->name);

  return 1;
});

/**
 * @brief Pops the current build context, restoring the provided one.
 * @param context The context to restore. Can be NULL if it was the top-level context.
 */
void build_context_pop(build_context_t *context) impl({
  if (context) {
    info("exiting \x1b[1m\x1b[34m%s\033[m... returning to \x1b[1m\x1b[34m%s\033[m",
        build_context->name, context->name);
  } else if (build_context) {
    info("exiting \x1b[1m\x1b[34m%s\033[m...", build_context->name);
  }

  build_context = context;
});

//////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Utilities //////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/**
 * @brief Check if `source_file` is older than `target_file`.
 *
 * If one of the files doesn't exist, this function will return false.
 * @param source_file Path to the first file to compare.
 * @param target_file Path to the second file to compare.
 * @return `true` if `source_file` has an older modification time than `target_file`, `false` otherwise.
 */
bool is_file_older(char *source_file, char *target_file) impl({
  time_t source_time, target_time;
  struct stat st;

  if (stat(source_file, &st) != 0) {
    return false;
  }

  source_time = st.st_mtim.tv_sec;

  if (stat(target_file, &st) != 0) {
    return false;
  }

  target_time = st.st_mtim.tv_sec;

  return source_time < target_time;
});

/**
 * @brief Searches for an executable in the system's PATH environment variable.
 * @param name The name of the executable to find.
 * @param output_buffer A buffer of at least `PATH_MAX` size to store the full path if found.
 * @return `true` if the executable was found, `false` otherwise.
 */
bool find_executable(const char *name, char output_buffer[PATH_MAX]) impl({
  char *path_env = getenv("PATH");

  struct stat st = {0};
  char *current_path = path_env;
  size_t name_len = strlen(name);

  while (current_path) {
    char *next_colon = strchr(current_path, ':');

    size_t current_path_len = next_colon
                                ? (size_t)((uintptr_t)next_colon - (uintptr_t)current_path)
                                : strlen(current_path);

    memset(output_buffer, 0, sizeof(char[PATH_MAX]));

    memcpy(output_buffer, current_path, current_path_len);
    output_buffer[current_path_len] = '/';
    memcpy(&output_buffer[current_path_len + 1], name, name_len);

    output_buffer[current_path_len + name_len + 2] = '\0';

    if (stat(output_buffer, &st) == 0) {
      bool is_executable =
        (S_ISREG(st.st_mode) ||
         S_ISLNK(st.st_mode)) && // Check if it is a regular file
        (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0; // Check if it has executable permissions

      if (is_executable) {
        return true;
      }
    }

    current_path = (char *)((uintptr_t)next_colon + (uintptr_t)1);
  }

  *output_buffer = 0;

  return false;
});

//////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Processes //////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/** @brief A dynamic list to store process IDs (pid_t). */
typedef struct pid_list_t {
  /** @brief The allocated capacity of the list. */
  size_t size,
  /** @brief The current number of items in the list. */
         current;
  /** @brief The array of process IDs. */
  pid_t *items;
} pid_list_t;

/**
 * @brief Creates and initializes a new, empty process ID list.
 * @return A pointer to the newly allocated `pid_list_t`. Must be freed with `pid_list_free`.
 */
pid_list_t *pid_list_create()
  impl({ return (pid_list_t *)calloc(1, sizeof(pid_list_t)); });

/**
 * @brief Adds a process ID to the list, resizing if necessary.
 * @param list The PID list to add to.
 * @param pid The process ID to add. If -1, it is ignored.
 */
void pid_list_add(pid_list_t *list, pid_t pid) impl({
  if (pid == -1)
    return;

  if (list->size <= list->current) {
    list->size = (list->size + 1) * 2;

    list->items = (pid_t *)realloc(list->items, sizeof(pid_t) * list->size);
  }

  list->items[list->current++] = pid;
});

/**
 * @brief Frees the memory used by a process ID list.
 * @param list The list to free.
 */
void pid_list_free(pid_list_t *list) impl({
  list->size = list->current = 0;

  free(list->items);
  free(list);
});

/**
 * @brief Waits for a single process to terminate and returns its exit code.
 * @param pid The ID of the process to wait for.
 * @return The exit status of the process, or the signal number if it was terminated by a signal.
 */
int wait_pid_sync(pid_t pid) impl({
  int status;

  while (-1 != waitpid(pid, &status, 0)) {
    if (WIFSIGNALED(status)) {
      error("process %d received signal '%s'", pid, strsignal(WTERMSIG(status)));

      return WTERMSIG(status);
    }

    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
  }

  return -1;
});

/**
 * @brief Waits for all processes in a PID list to terminate.
 * @param pids The list of process IDs to wait for.
 * @return A bitwise-OR combination of all process exit statuses.
 */
int pid_list_wait_sync(pid_list_t *pids) impl({
  int status = 0;

  for (size_t i = 0; i < pids->size; i++) {
    pid_t pid = pids->items[i];

    while (-1 != waitpid(pid, &status, 0)) {
      if (WIFSIGNALED(status)) {
        error("process %d received signal '%s'", pid, strsignal(WTERMSIG(status)));

        status |= WTERMSIG(status);
      }

      if (WIFEXITED(status)) {
        status |= WEXITSTATUS(status);
      }
    }
  }

  return status;
});

/**
 * @brief Creates a new child process to run a command.
 * @param path The name or path of the executable to run.
 * @param argv The argument vector (list of strings) for the new process, ending with NULL.
 * @return The process ID of the child on success, or -1 on failure.
 */
pid_t run_command(const char *path, char **argv) impl({
  char executable_path[PATH_MAX];

  if (*path == '/') {
    // Name is an absolute path
    memcpy(executable_path, path, strlen(path) + 1);
  } else if (!find_executable(path, executable_path)) {
    error("run_command: couldn't find executable %s in PATH.", path);

    return -1;
  }

  pid_t pid = fork();

  if (pid == 0) {
    if (-1 == execv(executable_path, (char *const *)argv)) {
      error("execv failed: %s", strerror(errno));
    }

    exit(127);
  }

  if (pid == -1) {
    error("fork() failed: %s", strerror(errno));

    return -1;
  }

  return pid;
});

//////////////////////////////////////////////////////////////////////////////
////////////////////////////// Argument parser ///////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/**
 * @brief Gets the program name.
 * @return The name of the program.
 */
char *builder_program_name() impl({
  return program_name;
});

impl(
  /**
   * @brief A node for the linked list of parsed arguments.
   */
  typedef struct argument_node_t {
    /** @brief The parsed argument data. */
    struct argument_t *argument;
    /** @brief Pointer to the next node in the list. */
    struct argument_node_t *next;
  } argument_node_t;

  /**
   * @brief Finds an argument definition by its long name (e.g., "help").
   * @param name The long name to search for.
   * @param defs The array of argument definitions.
   * @param defs_count The number of definitions in the array.
   * @return A pointer to the matching definition, or NULL if not found.
   */
  static const argument_definition_t* builder_find_def_by_long_name(const char *name, const argument_definition_t defs[], size_t defs_count) {
    for (size_t i = 0; i < defs_count; ++i) {
      if (defs[i].longName && strcmp(name, defs[i].longName) == 0) {
        return &defs[i];
      }
    }
    return NULL;
  }

  /**
   * @brief Finds an argument definition by its short name (e.g., 'h').
   * @param name The short name character to search for.
   * @param defs The array of argument definitions.
   * @param defs_count The number of definitions in the array.
   * @return A pointer to the matching definition, or NULL if not found.
   */
  static const argument_definition_t* builder_find_def_by_short_name(char name, const argument_definition_t defs[], size_t defs_count) {
    for (size_t i = 0; i < defs_count; ++i) {
      if (defs[i].shortName != '\0' && defs[i].shortName == name) {
        return &defs[i];
      }
    }
    return NULL;
  }

  /**
   * @brief Allocates and adds a new parsed argument to the linked list.
   * @param args The main arguments structure containing the list.
   * @param def The definition of the argument being added.
   * @param value The string value for the argument, or NULL for toggles.
   * @return `true` on success, `false` on memory allocation failure.
   */
  static bool builder_add_argument(arguments_t *args, const argument_definition_t* def, char* value) {
    argument_t *new_arg = (typeof(new_arg)) malloc(sizeof(argument_t));

    if (!new_arg) return false;

    new_arg->longName = def->longName;
    new_arg->shortName = def->shortName;
    new_arg->value = value;

    argument_node_t *new_node = (typeof(new_node)) malloc(sizeof(argument_node_t));
    if (!new_node) {
      free(new_arg);
      return false;
    }
    new_node->argument = new_arg;
    new_node->next = NULL;

    // Append to the list
    if (args->head == NULL) {
      args->head = new_node;
    } else {
      argument_node_t *current = args->head;
      while (current->next != NULL) {
        current = current->next;
      }
      current->next = new_node;
    }
    return true;
  }
)

/**
 * @brief Parses command-line arguments (argc, argv) based on a list of definitions.
 *
 * @param argc The argument count from main().
 * @param argv The argument vector from main().
 * @param defs An array of argument_definition_t that defines valid arguments.
 * @param defs_count The number of elements in the defs array.
 * @return A pointer to a newly allocated arguments_t struct, or NULL on error.
 * The caller is responsible for freeing this struct using builder_free_arguments().
 */
arguments_t* builder_parse_arguments(int argc, char **argv, const argument_definition_t *defs, size_t defs_count) impl({
  if (defs_count <= 0) return NULL;

  arguments_t *parsed_args = (typeof(parsed_args)) calloc(1, sizeof(arguments_t));

  if (!parsed_args) {
    perror("Failed to allocate memory for arguments");
    return NULL;
  }

  int i;
  for (i = 1; i < argc; ++i) {
    char *arg = argv[i];

    // Stop parsing if "--" is encountered.
    if (strcmp(arg, "--") == 0) {
      i++; // Move past the "--"
      break;
    }

    // Stop parsing if the argument doesn't start with '-'.
    if (arg[0] != '-') {
      break;
    }

    // --- Handle Long Options (e.g., --help) ---
    if (strncmp(arg, "--", 2) == 0) {
      char *long_name = arg + 2;
      if (strlen(long_name) == 0) { // Argument is just "--"
        i++;
        break;
      }

      const argument_definition_t* def = builder_find_def_by_long_name(long_name, defs, defs_count);
      if (!def) {
        fprintf(stderr, "Error: Unknown option %s\n", arg);
        free(parsed_args);
        return NULL;
      }

      if (def->requiresValue) {
        if (i + 1 >= argc) {
          fprintf(stderr, "Error: Option %s requires a value, but none was supplied.\n", arg);
          free(parsed_args);
          return NULL;
        }
        if (!builder_add_argument(parsed_args, def, argv[i + 1])) return NULL;
        i++; // Consume the value argument
      } else { // It's a toggle
        if (!builder_add_argument(parsed_args, def, NULL)) return NULL;
      }
    }
    // --- Handle Short Options (e.g., -h, -xvf) ---
    else {
      char *short_opts = arg + 1;
      bool value_option_found = false;

      for (int j = 0; short_opts[j] != '\0'; ++j) {
        const argument_definition_t* def = builder_find_def_by_short_name(short_opts[j], defs, defs_count);
        if (!def) {
          fprintf(stderr, "Error: Unknown option -%c in %s\n", short_opts[j], arg);
          free(parsed_args);
          return NULL;
        }

        if (def->requiresValue) {
          if (value_option_found) {
            fprintf(stderr, "Error: Only one option requiring a value is allowed in a single group like %s.\n", arg);
            free(parsed_args);
            return NULL;
          }
          value_option_found = true;

          // Check if value is attached (e.g., -f<value>)
          if (short_opts[j + 1] != '\0') {
            if (!builder_add_argument(parsed_args, def, &short_opts[j + 1])) return NULL;
            goto next_arg; // Done with this argv element
          }
          // Else, the value must be the next argument
          else {
            if (i + 1 >= argc) {
              fprintf(stderr, "Error: Option -%c requires a value, but none was supplied.\n", def->shortName);
              free(parsed_args);
              return NULL;
            }
            if (!builder_add_argument(parsed_args, def, argv[i + 1])) return NULL;
            i++; // Consume the value argument
          }
        } else { // It's a toggle
          if (!builder_add_argument(parsed_args, def, NULL)) return NULL;
        }
      }
    }
next_arg:;
  }

  // Store the remaining non-option arguments
  parsed_args->non_option_argc = argc - i;
  parsed_args->non_option_argv = argv + i;

  return parsed_args;
});

/**
 * @brief Frees all memory associated with an arguments_t struct.
 * @param args The arguments structure to free.
 */
void builder_free_arguments(arguments_t *args) impl({
  if (!args) return;

  argument_node_t *current = args->head;

  while (current != NULL) {
    argument_node_t *next = current->next;

    // The value is a pointer to argv, not heap-allocated, so we don't free it.
    free(current->argument);
    free(current);

    current = next;
  }

  free(args);
});
#endif /** __BUILDER_UNIX_H__ */
