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

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "gclue-gpsd-source.h"
#include "gclue-location.h"
#include "config.h"
#include "gclue-enum-types.h"

struct _GClueGpsdSourcePrivate {
        GCancellable *cancellable;

        DBusConnection *system_bus;
};

G_DEFINE_TYPE_WITH_CODE (GClueGpsdSource,
                         gclue_gpsd_source,
                         GCLUE_TYPE_LOCATION_SOURCE,
                         G_ADD_PRIVATE (GClueGpsdSource))

static GClueLocationSourceStartResult
gclue_gpsd_source_start (GClueLocationSource *source);
static GClueLocationSourceStopResult
gclue_gpsd_source_stop (GClueLocationSource *source);

static void
connect_to_service (GClueGpsdSource *source);
static void
disconnect_from_service (GClueGpsdSource *source);

static void
set_location (GClueLocation *location,
                 gpointer user_data)
{
        GClueGpsdSource *source = GCLUE_GPSD_SOURCE (user_data);
        gclue_location_source_set_location (GCLUE_LOCATION_SOURCE (source), location);
}

static int
on_signal (int unused,
           DBusMessage *msg,
           gpointer obj)
{
        DBusError error;
        double time;
        int mode;
        double time_uncertainty;
        double latitude;
        double longitude;
        double horizontal_uncertainty;
        double altitude;
        double altitude_uncertainty;
        double course;
        double course_uncertainty;
        double speed;
        double speed_uncertainty;
        double climb;
        double climb_uncertainty;
        const char* name;
        const char *member;

        (void) obj;
        (void) unused;

        member = dbus_message_get_member (msg);
        if (strcmp (member, "fix") != 0)
                return 0;

        dbus_error_init (&error);

        if (!dbus_message_get_args (msg, &error,
                                    DBUS_TYPE_DOUBLE, &time,
                                    DBUS_TYPE_INT32, &mode,
                                    DBUS_TYPE_DOUBLE, &time_uncertainty,
                                    DBUS_TYPE_DOUBLE, &latitude,
                                    DBUS_TYPE_DOUBLE, &longitude,
                                    DBUS_TYPE_DOUBLE, &horizontal_uncertainty,
                                    DBUS_TYPE_DOUBLE, &altitude,
                                    DBUS_TYPE_DOUBLE, &altitude_uncertainty,
                                    DBUS_TYPE_DOUBLE, &course,
                                    DBUS_TYPE_DOUBLE, &course_uncertainty,
                                    DBUS_TYPE_DOUBLE, &speed,
                                    DBUS_TYPE_DOUBLE, &speed_uncertainty,
                                    DBUS_TYPE_DOUBLE, &climb,
                                    DBUS_TYPE_DOUBLE, &climb_uncertainty,
                                    DBUS_TYPE_STRING, &name,
                                    NULL)) {
                dbus_error_free (&error);
        } else {
                GClueLocation *location;
                location = gclue_location_new_full (latitude,
                                                    longitude,
                                                    horizontal_uncertainty,
                                                    speed,
                                                    course,
                                                    altitude,
                                                    time,
                                                    "GPSD location");

                g_debug ("GPSD: Location timestamp: %" G_GUINT64_FORMAT, (guint64) time);
                g_debug ("GPSD: Latitude: %f, Longitude: %f", latitude, longitude);
                g_debug ("GPSD: Altitude: %f meters", altitude);
                g_debug ("GPSD: Speed: %f knots", speed);
                g_debug ("GPSD: Bearing: %f degrees", course);
                g_debug ("GPSD: Horizontal Accuracy: %f meters", horizontal_uncertainty);
                g_debug ("GPSD: Vertical Accuracy: %f meters", altitude_uncertainty);

                set_location (location, obj);
                g_object_unref (location);
        }

        return 0;
}

static void
connect_to_service (GClueGpsdSource *source)
{
        GClueGpsdSourcePrivate *priv = source->priv;

        g_cancellable_reset (priv->cancellable);

	priv->system_bus = dbus_bus_get_private (DBUS_BUS_SYSTEM, NULL);
	dbus_connection_setup_with_g_main (priv->system_bus, NULL);

	dbus_bus_add_match (priv->system_bus,
                            "type='signal',interface='org.gpsd'",
                            NULL);

	dbus_connection_add_filter (priv->system_bus,
                                    (DBusHandleMessageFunction) on_signal,
                                    source,
                                    NULL);
}

static void
disconnect_from_service (GClueGpsdSource *source)
{
        GClueGpsdSourcePrivate *priv = source->priv;

        g_cancellable_cancel (priv->cancellable);

        if (priv->system_bus) {
                dbus_connection_close(priv->system_bus);
                dbus_connection_unref(priv->system_bus);
                priv->system_bus = NULL;
        }
}

static void
gclue_gpsd_source_finalize (GObject *ggpsd)
{
        GClueGpsdSourcePrivate *priv = GCLUE_GPSD_SOURCE (ggpsd)->priv;

        G_OBJECT_CLASS (gclue_gpsd_source_parent_class)->finalize (ggpsd);

        g_clear_object (&priv->cancellable);

        if (priv->system_bus) {
                dbus_connection_close (priv->system_bus);
                dbus_connection_unref (priv->system_bus);
                priv->system_bus = NULL;
        }
}

static void
gclue_gpsd_source_class_init (GClueGpsdSourceClass *klass)
{
        GClueLocationSourceClass *source_class = GCLUE_LOCATION_SOURCE_CLASS (klass);
        GObjectClass *ggpsd_class = G_OBJECT_CLASS (klass);

        ggpsd_class->finalize = gclue_gpsd_source_finalize;

        source_class->start = gclue_gpsd_source_start;
        source_class->stop = gclue_gpsd_source_stop;
}

static void
gclue_gpsd_source_init (GClueGpsdSource *source)
{
        GClueGpsdSourcePrivate *priv;

        source->priv = gclue_gpsd_source_get_instance_private (source);
        priv = source->priv;

        priv->cancellable = g_cancellable_new ();

        GClueAccuracyLevel level;
        level = GCLUE_ACCURACY_LEVEL_EXACT;
        g_debug ("Setting accuracy level to %s: %u",
                 G_OBJECT_TYPE_NAME(source), level);
        g_object_set (G_OBJECT(source),
                      "available-accuracy-level", level, NULL);

        connect_to_service (source);
}

/**
 * gclue_gpsd_source_get_singleton:
 *
 * Get the #GClueGpsdSource singleton.
 *
 * Returns: (transfer full): a new ref to #GClueGpsdSource. Use g_object_unref()
 * when done.
 **/
GClueGpsdSource *
gclue_gpsd_source_get_singleton (void)
{
        static GClueGpsdSource *source = NULL;

        if (source == NULL) {
                source = g_object_new (GCLUE_TYPE_GPSD_SOURCE, NULL);
                g_object_add_weak_pointer (G_OBJECT (source),
                                           (gpointer) &source);
        } else
                g_object_ref (source);

        return source;
}

static GClueLocationSourceStartResult
gclue_gpsd_source_start (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;
        GClueLocationSourceStartResult base_result;

        g_return_val_if_fail (GCLUE_IS_GPSD_SOURCE (source),
                              GCLUE_LOCATION_SOURCE_START_RESULT_FAILED);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_gpsd_source_parent_class);
        base_result = base_class->start (source);
        if (base_result == GCLUE_LOCATION_SOURCE_START_RESULT_FAILED)
                return base_result;

        connect_to_service (GCLUE_GPSD_SOURCE (source));

        return TRUE;
}

static GClueLocationSourceStopResult
gclue_gpsd_source_stop (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;
        GClueLocationSourceStopResult base_result;

        g_return_val_if_fail (GCLUE_IS_GPSD_SOURCE (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_gpsd_source_parent_class);
        base_result = base_class->stop (source);
        if (base_result == GCLUE_LOCATION_SOURCE_STOP_RESULT_STILL_USED)
                return base_result;

        disconnect_from_service (GCLUE_GPSD_SOURCE (source));

        return TRUE;
}
