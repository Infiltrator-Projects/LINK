// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_FEATURES_H
#define LINK_FEATURES_H

/*
 * Optional provider implementations. LINK keeps established providers enabled
 * by default, while product faces that cannot use a provider may compile the
 * shared engine without dragging that provider's implementation into the
 * product binary.
 */
#ifndef LINK_ENABLE_MERCEDES_ME_NATIVE
#define LINK_ENABLE_MERCEDES_ME_NATIVE 1
#endif

#endif
