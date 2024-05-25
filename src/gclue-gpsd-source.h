/* vim: set et ts=8 sw=8: */
/*
 * Copyright 2024 Bardia Moshiri
 *
 * Geoclue is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Geoclue is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Geoclue; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Bardia Moshiri <fakeshell@bardia.tech>
 */

#ifndef GCLUE_GPSD_SOURCE_H
#define GCLUE_GPSD_SOURCE_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-location-source.h"

G_BEGIN_DECLS

GType gclue_gpsd_source_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_GPSD_SOURCE            (gclue_gpsd_source_get_type ())
#define GCLUE_GPSD_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_GPSD_SOURCE, GClueGpsdSource))
#define GCLUE_IS_GPSD_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_GPSD_SOURCE))
#define GCLUE_GPSD_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_GPSD_SOURCE, GClueGpsdSourceClass))
#define GCLUE_IS_GPSD_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_GPSD_SOURCE))
#define GCLUE_GPSD_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_GPSD_SOURCE, GClueGpsdSourceClass))

/**
 * GClueGpsdSource:
 *
 * All the fields in the #GClueGpsdSource structure are private and should never be accessed directly.
**/
typedef struct _GClueGpsdSource GClueGpsdSource;
typedef struct _GClueGpsdSourceClass GClueGpsdSourceClass;
typedef struct _GClueGpsdSourcePrivate GClueGpsdSourcePrivate;

struct _GClueGpsdSource {
        /* <private> */
        GClueLocationSource parent_instance;
        GClueGpsdSourcePrivate *priv;
};

/**
 * GClueGpsdSourceClass:
 *
 * All the fields in the #GClueGpsdSourceClass structure are private and should never be accessed directly.
**/
struct _GClueGpsdSourceClass {
        /* <private> */
        GClueLocationSourceClass parent_class;
};

GClueGpsdSource *gclue_gpsd_source_get_singleton (void);

G_END_DECLS

#endif /* GCLUE_GPSD_SOURCE_H */
