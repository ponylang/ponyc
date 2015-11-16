#ifndef PKG_BUILDFLAGSET_H
#define PKG_BUILDFLAGSET_H

#include <stdbool.h>
#include <stddef.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

/* A build flag set records the platform and user build flags required for some
 * ifdef and/or use guard expression and enumerates through the possible
 * configs for those flags.
 *
 * To use a build flag set first add all the required symbols with the `add`
 * function and then enumerate with the `startenum` and `next` functions. Once
 * `startenum` is called no more flags may be added.
 *
 * The number of possible configs can be read at any time and is intended for
 * warnings to the user when there are many configs to check, which might take
 * a long time.
 *
 * Note that some platform flags, such as the OS flags, are mutually exclusive
 * and build flag sets know this. Thus if any OS flag is added all known OSes
 * will be included in the enumeration.
 *
 * All user flags are assumed to be independent.
 *
 * Although platform and user flags are presented with slightly different
 * syntax in the Pony source, we require them to be in a single namespace.
 * Therefore build flag sets do not distinguish between them.
 *
 * All flag names must be passed as stringtab'ed strings.
 */

typedef struct buildflagset_t buildflagset_t;


// Create a build flag set.
// The returned set must be freed with buildflagset_free() later.
buildflagset_t* buildflagset_create();

// Free the given build flag set.
// NULL may be safely passed for the set.
void buildflagset_free(buildflagset_t* set);

// Add the specified build flag to the given set.
// The flag name must be stringtab'ed.
void buildflagset_add(buildflagset_t* set, const char* flag);

// Report how many possible configurations there are in this set.
// Intended for warning the user about potentially very long compiles.
// The value is returned as a double to allow for sense reporting even for very
// large results.
double buildflagset_configcount(buildflagset_t* set);

// Start enumerating through the possible configs in this set.
// Once this is called no more flags may be added.
// buildflagset_next() must be called after this to get the first config.
// May be called more than once to restart enumeration.
void buildflagset_startenum(buildflagset_t* set);

// Move on to the next config in this enumeration.
// Returns: true if next state enumerated, false if there are no more configs.
bool buildflagset_next(buildflagset_t* set);

// Report whether the specified build flag is in the current config in the
// given set. Must only be called after the enumeration is started by calling
// buildflagset_first() and only for flags previously added.
// The flag name must be stringtab'ed.
bool buildflagset_get(buildflagset_t* set, const char* flag);

// Print out this build flag set, for error messages, etc.
// The return text is within a buffer that belongs to the build flag set. It
// must not be freed and is only valid until print is called again on the same
// build flag set or that build flag set is freed.
const char* buildflagset_print(buildflagset_t* set);


// User build flags are defined on the ponyc command line and used to select
// code options with ifdef expressions.

// Define the given user build flag.
// Returns: true on success, false on duplicate flag.
bool define_build_flag(const char* name);

// Report whether the given user build flag is defined.
bool is_build_flag_defined(const char* name);

PONY_EXTERN_C_END

#endif
