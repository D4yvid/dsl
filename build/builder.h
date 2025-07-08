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

#ifndef BUILDER_CONTEXT_DATA
#define BUILDER_CONTEXT_DATA void *
#endif

#ifdef BUILDER_NO_IMPLEMENTATION
#define impl(...)
#else
#define impl(...) __VA_ARGS__
#endif

#define StringArray(...) ((char *[]){__VA_ARGS__})
#define StringArrayN(...) ((char *[]){__VA_ARGS__, NULL})

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

typedef struct argument_definition_t {
  const char *longName;   // e.g., "help", can be NULL if shortName is set
  char shortName;         // e.g., 'h', can be '\0' if longName is set
  bool requiresValue;     // True if the option needs a value (e.g., -f <file>)
  bool toggleOption;      // True if it's a switch (e.g., --verbose)
} argument_definition_t;

/**
 * @brief Represents a parsed argument found on the command line.
 */
typedef struct argument_t {
  const char *longName;   // Will point to the definition's longName
  char shortName;         // Will be the definition's shortName
  char *value;            // The value if the option requires one, otherwise NULL
} argument_t;

/**
 * @brief The main container for all parsed arguments, holding the linked list head.
 */
typedef struct arguments_t {
  struct argument_node_t *head;
  int non_option_argc;      // Count of arguments after parsing stopped
  char **non_option_argv;   // Pointer to the start of non-option arguments
} arguments_t;

#if defined __unix__
#include "./builder.unix.h"
#elif defined _WIN32 || defined WIN32
#error "Windows is not supported (doesn't have a implementation) for now!"
#else
#error "Unknown platform (please send a PR with the platform you're using!)"
#endif

#define entrypoint(args)                              \
  void builder_entrypoint(arguments_t *args);         \
  int main main_impl                                  \
  void builder_entrypoint(arguments_t *args)

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
