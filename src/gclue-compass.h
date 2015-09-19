/* vim: set et ts=8 sw=8: */
/*
 * Copyright (C) 2015 Red Hat, Inc.
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
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#ifndef GCLUE_COMPASS_H
#define GCLUE_COMPASS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCLUE_TYPE_COMPASS            (gclue_compass_get_type())
#define GCLUE_COMPASS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_COMPASS, GClueCompass))
#define GCLUE_COMPASS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_COMPASS, GClueCompass const))
#define GCLUE_COMPASS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_COMPASS, GClueCompassClass))
#define GCLUE_IS_COMPASS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_COMPASS))
#define GCLUE_IS_COMPASS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_COMPASS))
#define GCLUE_COMPASS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_COMPASS, GClueCompassClass))

typedef struct _GClueCompass        GClueCompass;
typedef struct _GClueCompassClass   GClueCompassClass;
typedef struct _GClueCompassPrivate GClueCompassPrivate;

struct _GClueCompass
{
        GObject parent;

        /*< private >*/
        GClueCompassPrivate *priv;
};

struct _GClueCompassClass
{
        GObjectClass parent_class;
};

GType gclue_compass_get_type (void) G_GNUC_CONST;

GClueCompass *
gclue_compass_get_singleton (void);
gdouble
gclue_compass_get_heading   (GClueCompass *compass);

G_END_DECLS

#endif /* GCLUE_COMPASS_H */
