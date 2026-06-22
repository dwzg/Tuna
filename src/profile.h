/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

/**
 * @file   profile.h
 * @brief  Zero-cost cycle-profiling markers for the per-stage benchmark.
 */

#ifndef PROFILE_H_
#define PROFILE_H_

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*                         DEFINITIONS AND MACROS                            */
/*---------------------------------------------------------------------------*/
/**
 * @brief Mark a DSP stage boundary for the cycle profiler.
 *
 * Writes a small stage id to GPIOR0, a side-effect-free general-purpose
 * register. The simavr-based profiler (bench/simprof.c) latches the CPU cycle
 * counter on each write, so the cycles between two markers are the cost of the
 * stage between them. It is enabled only when TUNA_PROFILE is defined (the
 * bench/profile_stages.sh build); in every normal firmware or host-test build
 * it expands to nothing, so the production code is byte-for-byte unchanged.
 *
 * Place markers only at boundaries that execute once per call (not inside the
 * inner loops) so each id is written exactly once and in ascending order.
 */
#if defined(TUNA_PROFILE) && defined(__AVR__)
#include <avr/io.h>
#define PROFILE_MARK(stage) (GPIOR0 = (uint8_t)(stage))
#else
#define PROFILE_MARK(stage) ((void)0)
#endif

/*---------------------------------------------------------------------------*/
/*                                  EOF                                      */
/*---------------------------------------------------------------------------*/

#endif /* PROFILE_H_ */
