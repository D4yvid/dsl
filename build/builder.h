/**
 * @file builder.h
 * @brief The primary public-facing header for the builder framework.
 *
 * This file defines the core data structures, macros, and platform-agnostic
 * interfaces for creating build scripts. It includes the platform-specific
 * backend implementation.
 */
#ifndef BUILDER_H
#define BUILDER_H

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

/**
 * @brief Defines the type for user-defined data within a `build_context_t`.
 * Users can override this by defining it before including builder.h to use a
 * custom struct instead of the default `void *`.
 */
#ifndef BUILDER_CONTEXT_DATA
#define BUILDER_CONTEXT_DATA void *
#endif

/**
 * @brief Conditionally includes or excludes implementation code.
 * If `BUILDER_NO_IMPLEMENTATION` is defined, this macro expands to nothing,
 * effectively hiding function bodies. Otherwise, it expands to its arguments.
 * This allows the library to be used in a "header-only" fashion where one
 * source file defines `BUILDER_IMPLEMENTATION` to generate the object code.
 */
#ifdef BUILDER_NO_IMPLEMENTATION
#define impl(...)
#else
#define impl(...) __VA_ARGS__
#endif

/**
 * @brief A convenience macro to create a `char *` array from string literals.
 * @param ... A comma-separated list of string literals.
 */
#define StringArray(...) ((char *[]){__VA_ARGS__})

/**
 * @brief Creates a `char *` array from string literals, appending a NULL terminator.
 * This is useful for passing arguments to `execv`-style functions.
 * @param ... A comma-separated list of string literals.
 */
#define StringArrayN(...) ((char *[]){__VA_ARGS__, NULL})

/**
 * @brief A convenience macro to create an `argument_definition_t` using designated initializers.
 * @param ... The initializers for the struct (e.g., `.longName = "help", .shortName = 'h'`).
 */
#define Argument(...) ((argument_definition_t) { __VA_ARGS__ })

/**
 * @brief A macro to iterate over each parsed argument in an arguments_t struct.
 *
 * @param args_ptr A pointer to the arguments_t struct to iterate over.
 * @param arg_var The variable name (argument_t*) to hold the current argument in each loop iteration.
 */
#define arguments_foreach(args_ptr, arg_var) \
    for (argument_node_t* _node = (args_ptr)->head; \
         _node != NULL && ((arg_var) = _node->argument); \
         _node = _node->next)

/**
 * @brief Defines a valid command-line argument that can be parsed.
 */
typedef struct argument_definition_t {
  /** @brief The long name of the option (e.g., "help"). Can be NULL if `shortName` is set. */
  const char *longName;
  /** @brief The short, single-character name of the option (e.g., 'h'). Can be `\0` if `longName` is set. */
  char shortName;
  /** @brief `true` if the option requires a value (e.g., `-f <file>`). */
  bool requiresValue;
  /** @brief `true` if the option is a simple switch/toggle (e.g., `--verbose`). */
  bool toggleOption;
} argument_definition_t;

/**
 * @brief Represents a parsed argument found on the command line.
 */
typedef struct argument_t {
  /** @brief Points to the `longName` from the corresponding definition. */
  const char *longName;
  /** @brief The `shortName` from the corresponding definition. */
  char shortName;
  /** @brief The value provided for the argument, or NULL for a toggle option. */
  char *value;
} argument_t;

/**
 * @brief The main container for all parsed arguments.
 */
typedef struct arguments_t {
  /** @brief The head of the linked list of parsed arguments.
    * @note The internal `argument_node_t` is defined in the platform-specific backend. */
  struct argument_node_t *head;
  /** @brief The count of command-line arguments that were not parsed as options. */
  int non_option_argc;
  /** @brief A pointer to the start of non-option arguments in the original `argv` vector. */
  char **non_option_argv;
} arguments_t;

/**
 * @brief This block includes the platform-specific implementation file.
 * It selects the correct backend based on predefined compiler macros.
 */
#if defined __unix__
#include "./builder.unix.h"
#elif defined _WIN32 || defined WIN32
#error "Windows is not supported (doesn't have a implementation) for now!"
#else
#error "Unknown platform (please send a PR with the platform you're using!)"
#endif

/**
 * @brief Defines the main entry point for the build script.
 * This macro sets up the necessary `main` function and declares `builder_entrypoint`,
 * which the user must implement to define the build logic.
 *
 * @param args The name of the `arguments_t *` variable that will be passed to `builder_entrypoint`.
 * @example
 * entrypoint(args) {
 * // Your build logic here...
 * // You can use the `args` variable.
 * }
 */
#define entrypoint(args)                                  \
  void builder_entrypoint(arguments_t *args);               \
  int main main_impl                                        \
  void builder_entrypoint(arguments_t *args)

/**
 * @brief This section verifies that the included backend has implemented all required macros.
 * If a required macro is not defined by the backend, a compile-time error is generated.
 */
#ifndef $
#error "The current backend doesn't implement the '$' macro!"
#endif

#ifndef $_sync
#error "The current backend doesn't implement the '$_sync' macro!"
#endif

#ifndef BuildContext
#error "The current backend doesn't implement the 'BuildContext' macro!"
#endif

#ifndef SyncGroup
#error "The current backend doesn't implement the 'SyncGroup' macro!"
#endif

#ifndef info
#error "The currnet backend doesn't implement the 'info' macro!"
#endif

#ifndef error
#error "The currnet backend doesn't implement the 'error' macro!"
#endif

#ifndef warn
#error "The currnet backend doesn't implement the 'warn' macro!"
#endif

#endif /** BUILDER_H */
