/*
 * This file is a part of MAFW
 *
 * Copyright (C) 2007, 2008, 2009 Nokia Corporation, all rights reserved.
 *
 * Contact: Visa Smolander <visa.smolander@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __MAIN_H__
#define __MAIN_H__

#include <libmafw/mafw.h>
#include <gtk/gtk.h>

void mtg_print_signal_gen (const gchar* origin, const gchar* signal,
			   const gchar* format, ...);
void mtg_print_signal (MafwExtension* origin, const gchar* signal,
		       const gchar* format, ...);

void
application_exit            (void);

void activate_all(gboolean make_active);

#endif /* __MAIN_H__ */
