// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_ABOUT_H
#define LINK_ABOUT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Product identity supplied to LINK-owned About renderers.
 *
 * The product owns these facts; LINK owns how they are presented on each
 * platform. All fields except product_name and version are optional. Authors
 * are separated by newline characters so one field supports either one author
 * or several without imposing a platform-specific container type.
 */
typedef struct LinkAboutInfo {
    const char *product_name;
    const char *subtitle;
    const char *version;
    const char *description;
    const char *release_date;
    const char *authors;
    const char *copyright;
    const char *website;
    const char *license_name;
    const char *license_text;
    const char *credits;
} LinkAboutInfo;

#ifdef __cplusplus
}
#endif

#endif
