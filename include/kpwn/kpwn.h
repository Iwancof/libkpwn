#ifndef _KPWN_H_
#define _KPWN_H_

// kpwn.h — umbrella header for libkpwn.
//
// A single `#include <kpwn/kpwn.h>` pulls in everything. The arch-specific
// modules are selected at compile time via the target triple.

#include <kpwn/colors.h>
#include <kpwn/crosscache.h>
#include <kpwn/flow.h>
#include <kpwn/hexdump.h>
#include <kpwn/kernel.h>
#include <kpwn/logger.h>
#include <kpwn/memory.h>
#include <kpwn/overwrite.h>
#include <kpwn/slog.h>
#include <kpwn/spray.h>
#include <kpwn/utils.h>

#if defined(__x86_64__)
#include <kpwn/x86_64/cpu.h>
#include <kpwn/x86_64/memory.h>
#include <kpwn/x86_64/side_channel.h>
#elif defined(__aarch64__)
#include <kpwn/aarch64/pauth.h>
#endif

#endif
