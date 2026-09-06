// SPDX-License-Identifier: GPL-3.0-or-later
/* LINK-owned Apple portable-core amalgamation for product faces. */
#include "link/features.h"

/*
 * A product may already compile LINK's i18n or ISO-TP implementation as a
 * dedicated Xcode source.  Explicit opt-outs let that product consume this
 * LINK-owned amalgamation without creating duplicate symbols while preserving
 * the default single-translation-unit topology for new product faces.
 */
#ifndef LINK_APPLE_PORTABLE_CORE_EXTERNAL_I18N
#define LINK_APPLE_PORTABLE_CORE_EXTERNAL_I18N 0
#endif
#ifndef LINK_APPLE_PORTABLE_CORE_EXTERNAL_ISOTP
#define LINK_APPLE_PORTABLE_CORE_EXTERNAL_ISOTP 0
#endif

#include "../../src/core/workspace.c"
#if !LINK_APPLE_PORTABLE_CORE_EXTERNAL_I18N
#include "../../src/core/i18n.c"
#include "../../src/core/i18n_platform.c"
#include "../../src/core/language_pack.c"
#endif
#include "../../src/core/units.c"
#include "../../src/core/fuel_economy.c"
#include "../../src/core/diagnostic_request.c"
#include "../../src/core/doip.c"
#include "../../src/core/diagnostic_flow.c"
#include "../../src/core/diagnostic_capability.c"
#if !LINK_APPLE_PORTABLE_CORE_EXTERNAL_ISOTP
#include "../../src/core/isotp.c"
#endif
#include "../../src/core/parameter.c"
#include "../../src/core/dashboard.c"
#include "../../src/core/scheduler.c"
#include "../../src/core/telemetry.c"
#include "../../src/core/session_trace.c"
#include "../../src/core/transport.c"

#if LINK_ENABLE_MERCEDES_ME_NATIVE
#include "../../src/core/mercedes_me_adapter.c"
#define read_u16_be link_apple_native_read_u16_be
#define write_u16_be link_apple_native_write_u16_be
#include "../../src/core/mercedes_me_native_protocol.c"
#undef read_u16_be
#undef write_u16_be
#include "../../src/core/mercedes_me_diagnostic.c"
#endif

#include "../../src/elm327/elm327.c"
#include "../../src/elm327/can.c"
#include "../../src/elm327/probe.c"
#include "../../src/elm327/session.c"
#include "../../src/kwp2000/kwp2000.c"
#include "../../src/discover/safety.c"
#include "../../src/discover/ecu_probe.c"
