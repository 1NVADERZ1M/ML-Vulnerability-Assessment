/* Portions Copyright (C) 2009-2020 Greenbone Networks GmbH
 * Portions Copyright (C) 2006 Software in the Public Interest, Inc.
 * Based on work Copyright (C) 1998 - 2006 Tenable Network Security, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file pluginlaunch.h
 * @brief pluginlaunch.c header.
 */

#ifndef __PLUGINLAUNCH_H__
#define __PLUGINLAUNCH_H__

#include "pluginload.h"      /* for struct pl_class_t */
#include "pluginscheduler.h" /* for struct plugins_scheduler_t */

void
pluginlaunch_init (const char *);
void pluginlaunch_wait (kb_t);
void pluginlaunch_wait_for_free_process (kb_t);

void
pluginlaunch_stop (void);

int
plugin_launch (struct scan_globals *, struct scheduler_plugin *,
               struct in6_addr *, GSList *, kb_t, kb_t, nvti_t *);

void
pluginlaunch_disable_parallel_checks (void);
void
pluginlaunch_enable_parallel_checks (void);

int
wait_for_children (void);
#endif
