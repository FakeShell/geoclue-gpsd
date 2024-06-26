/* vim: set et ts=8 sw=8: */
/*
 * Copyright 2014 Red Hat, Inc.
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

#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <config.h>
#include "gclue-wifi.h"
#include "gclue-3g.h"
#include "gclue-config.h"
#include "gclue-error.h"
#include "gclue-mozilla.h"

#define WIFI_SCAN_TIMEOUT_HIGH_ACCURACY 10
/* Since this is only used for city-level accuracy, 5 minutes between each
 * scan is more than enough.
 */
#define WIFI_SCAN_TIMEOUT_LOW_ACCURACY  300

/* WiFi APs at and below this signal level in scan results are ignored.
 * In dBm units.
 */
#define WIFI_SCAN_BSS_NOISE_LEVEL -90

#define BSSID_LEN 6
#define BSSID_STR_LEN 17
#define MAX_SSID_LEN 32

/* Drop entries from the cache when they are more than 48 hours old. If we are
 * polling at high accuracy for that entire period, that gives a maximum cache
 * size of 17280 entries. At roughly 400B each, that’s about 7MB of heap for a
 * full cache (excluding overheads). */
#define CACHE_ENTRY_MAX_AGE_SECONDS (48 * 60 * 60)

/* The signal strength can typically vary by ±5 for a stationary laptop, so
 * match cache entries with that tolerance.
 * In dBm units.
 */
#define CACHE_ENTRY_MATCH_SIGNAL_WINDOW 10

/**
 * SECTION:gclue-wifi
 * @short_description: WiFi-based geolocation
 * @include: gclue-glib/gclue-wifi.h
 *
 * Contains functions to get the geolocation based on nearby WiFi networks.
 **/

static GClueLocationSourceStartResult
gclue_wifi_start (GClueLocationSource *source);
static GClueLocationSourceStopResult
gclue_wifi_stop (GClueLocationSource *source);

static guint
variant_hash (gconstpointer key);

static void
gclue_wifi_refresh_async (GClueWebSource      *source,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data);
static GClueLocation *
gclue_wifi_refresh_finish (GClueWebSource  *source,
                           GAsyncResult    *result,
                           GError         **error);

static void
disconnect_cache_prune_timeout (GClueWifi *wifi);

typedef struct {
        GArray *signals;
        GClueLocation *location;
} LocationCacheElement;

static LocationCacheElement *
location_cache_element_new (GArray *signals,
                            GClueLocation *location)
{
        LocationCacheElement *element;

        element = g_slice_new (LocationCacheElement);
        element->signals = signals;
        element->location = g_object_ref (location);
        return element;
}

static void location_cache_element_free (gpointer data)
{
        LocationCacheElement *element = data;

        if (element->signals)
                g_array_free (element->signals, TRUE);
        g_clear_object (&element->location);
        g_slice_free (LocationCacheElement, element);
}

typedef struct {
        GList *elements;
} LocationCacheValue;

static LocationCacheValue *
location_cache_value_new (void)
{
        LocationCacheValue *value;

        value = g_slice_new (LocationCacheValue);
        value->elements = NULL;
        return value;
}

static void location_cache_value_free (gpointer data)
{
        LocationCacheValue *value = data;

        g_list_free_full (value->elements, location_cache_element_free);
        g_slice_free (LocationCacheValue, value);
}

struct _GClueWifiPrivate {
        GCancellable *intf_cancellable, *bss_cancellable;
        GClueMozilla *mozilla;
        WPASupplicant *supplicant;
        WPAInterface *interface;
        GHashTable *bss_proxies;
        GHashTable *ignored_bss_proxies;
        gboolean bss_list_changed;

        gulong bss_added_id;
        gulong bss_removed_id;
        gulong scan_done_id;
        guint scan_wait_id;

        guint scan_timeout;

        GHashTable *location_cache;  /* (element-type GVariant LocationCacheValue) (owned) */
        guint cache_prune_timeout_id;
        guint cache_hits, cache_misses;

#if GLIB_CHECK_VERSION(2, 64, 0)
        GMemoryMonitor *memory_monitor;
        gulong low_memory_warning_id;
#endif
};

static SoupMessage *
gclue_wifi_create_query (GClueWebSource *source,
                         const char **query_data_description,
                         GError        **error);
static SoupMessage *
gclue_wifi_create_submit_query (GClueWebSource  *source,
                                GClueLocation   *location,
                                GError         **error);
static GClueAccuracyLevel
gclue_wifi_get_available_accuracy_level (GClueWebSource *source,
                                         gboolean        net_available);

G_DEFINE_TYPE_WITH_CODE (GClueWifi,
                         gclue_wifi,
                         GCLUE_TYPE_WEB_SOURCE,
                         G_ADD_PRIVATE (GClueWifi))

static void
disconnect_bss_signals (GClueWifi *wifi);
static void
on_scan_call_done (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data);
static void
on_scan_done (WPAInterface *object,
              gboolean      success,
              gpointer      user_data);

static void
gclue_wifi_finalize (GObject *gwifi)
{
        GClueWifi *wifi = (GClueWifi *) gwifi;

        G_OBJECT_CLASS (gclue_wifi_parent_class)->finalize (gwifi);

        g_cancellable_cancel (wifi->priv->intf_cancellable);

        disconnect_bss_signals (wifi);
        disconnect_cache_prune_timeout (wifi);

        g_clear_object (&wifi->priv->supplicant);
        g_clear_object (&wifi->priv->interface);
        g_clear_pointer (&wifi->priv->bss_proxies, g_hash_table_unref);
        g_clear_pointer (&wifi->priv->ignored_bss_proxies, g_hash_table_unref);
        g_clear_pointer (&wifi->priv->location_cache, g_hash_table_unref);
        g_clear_object (&wifi->priv->mozilla);
        g_clear_object (&wifi->priv->intf_cancellable);
}

static void
gclue_wifi_constructed (GObject *object);

static void
gclue_wifi_class_init (GClueWifiClass *klass)
{
        GClueWebSourceClass *web_class = GCLUE_WEB_SOURCE_CLASS (klass);
        GClueLocationSourceClass *source_class = GCLUE_LOCATION_SOURCE_CLASS (klass);
        GObjectClass *gwifi_class = G_OBJECT_CLASS (klass);

        source_class->start = gclue_wifi_start;
        source_class->stop = gclue_wifi_stop;
        web_class->refresh_async = gclue_wifi_refresh_async;
        web_class->refresh_finish = gclue_wifi_refresh_finish;
        web_class->create_submit_query = gclue_wifi_create_submit_query;
        web_class->create_query = gclue_wifi_create_query;
        web_class->get_available_accuracy_level =
                gclue_wifi_get_available_accuracy_level;
        gwifi_class->finalize = gclue_wifi_finalize;
        gwifi_class->constructed = gclue_wifi_constructed;
}

static void
on_bss_added (WPAInterface *object,
              const gchar  *path,
              GVariant     *properties,
              gpointer      user_data);

static guint
variant_to_string (GVariant *variant, guint max_len, char *ret)
{
        guint i;
        guint len;

        len = g_variant_n_children (variant);
        if (len == 0)
                return 0;
        g_return_val_if_fail(len <= max_len, 0);
        ret[len] = '\0';

        for (i = 0; i < len; i++)
                g_variant_get_child (variant,
                                     i,
                                     "y",
                                     &ret[i]);

        return len;
}

static guint
get_ssid_from_bss (WPABSS *bss, char *ssid)
{
        GVariant *variant = wpa_bss_get_ssid (bss);

        return variant_to_string (variant, MAX_SSID_LEN, ssid);
}

static gboolean
get_bssid_from_bss (WPABSS *bss, char *bssid)
{
        GVariant *variant;
        char raw_bssid[BSSID_LEN + 1] = { 0 };
        guint raw_len, i;

        variant = wpa_bss_get_bssid (bss);
        if (variant == NULL)
                return FALSE;

        raw_len = variant_to_string (variant, BSSID_LEN, raw_bssid);
        g_return_val_if_fail (raw_len == BSSID_LEN, FALSE);

        for (i = 0; i < BSSID_LEN; i++) {
                unsigned char c = (unsigned char) raw_bssid[i];

                if (i == BSSID_LEN - 1) {
                        g_snprintf (bssid + (i * 3), 3, "%02x", c);
                } else {
                        g_snprintf (bssid + (i * 3), 4, "%02x:", c);
                }
        }

        return TRUE;
}

static void
add_bss_proxy (GClueWifi *wifi,
               WPABSS    *bss)
{
        const char *path;

        path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (bss));
        if (g_hash_table_replace (wifi->priv->bss_proxies,
                                  g_strdup (path),
                                  bss)) {
                char ssid[MAX_SSID_LEN + 1] = { 0 };

                wifi->priv->bss_list_changed = TRUE;
                get_ssid_from_bss (bss, ssid);
                g_debug ("WiFi AP '%s' added.", ssid);
        }
}

static void
on_bss_signal_notify (GObject    *gobject,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        WPABSS *bss = WPA_BSS (gobject);
        const char *path;

        if (wpa_bss_get_signal (bss) <= WIFI_SCAN_BSS_NOISE_LEVEL) {
                char bssid[BSSID_STR_LEN + 1] = { 0 };

                get_bssid_from_bss (bss, bssid);
                g_debug ("WiFi AP '%s' still has very low strength (%d dBm)"
                         ", ignoring again…",
                         bssid,
                         wpa_bss_get_signal (bss));
                return;
        }

        g_signal_handlers_disconnect_by_func (G_OBJECT (bss),
                                              on_bss_signal_notify,
                                              user_data);
        add_bss_proxy (wifi, g_object_ref (bss));
        path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (bss));
        g_hash_table_remove (wifi->priv->ignored_bss_proxies, path);
}

static void
on_bss_proxy_ready (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
        GClueWifi *wifi;
        WPABSS *bss;
        g_autoptr(GError) error = NULL;
        char ssid[MAX_SSID_LEN + 1] = { 0 };

        bss = wpa_bss_proxy_new_for_bus_finish (res, &error);
        if (bss == NULL) {
                if (error && !g_error_matches (error, G_IO_ERROR,
                                               G_IO_ERROR_CANCELLED)) {
                        g_warning ("BSS proxy setup failed: %s",
                                   error->message);
                }

                return;
        }

        wifi = GCLUE_WIFI (user_data);

        if (gclue_mozilla_should_ignore_bss (bss)) {
                g_object_unref (bss);

                return;
        }

        get_ssid_from_bss (bss, ssid);
        g_debug ("Got WiFi AP '%s'", ssid);

        if (wpa_bss_get_signal (bss) <= WIFI_SCAN_BSS_NOISE_LEVEL) {
                const char *path;
                char bssid[BSSID_STR_LEN + 1] = { 0 };

                get_bssid_from_bss (bss, bssid);
                g_debug ("WiFi AP '%s' has very low strength (%d dBm)"
                         ", ignoring for now…",
                         bssid,
                         wpa_bss_get_signal (bss));
                g_signal_connect (G_OBJECT (bss),
                                  "notify::signal",
                                  G_CALLBACK (on_bss_signal_notify),
                                  user_data);
                path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (bss));
                g_hash_table_replace (wifi->priv->ignored_bss_proxies,
                                      g_strdup (path),
                                      bss);
                return;
        }

        add_bss_proxy (wifi, bss);
}

static void
on_bss_added (WPAInterface *object,
              const gchar  *path,
              GVariant     *properties,
              gpointer      user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);

        wpa_bss_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   "fi.w1.wpa_supplicant1",
                                   path,
                                   wifi->priv->bss_cancellable,
                                   on_bss_proxy_ready,
                                   wifi);
}

static gboolean
remove_bss_from_hashtable (const gchar *path, GHashTable *hash_table)
{
        char ssid[MAX_SSID_LEN + 1] = { 0 };
        WPABSS *bss = NULL;

        bss = g_hash_table_lookup (hash_table, path);
        if (bss == NULL)
                return FALSE;

        get_ssid_from_bss (bss, ssid);
        g_debug ("WiFi AP '%s' removed.", ssid);

        g_hash_table_remove (hash_table, path);

        return TRUE;
}

static void
on_bss_removed (WPAInterface *object,
                const gchar  *path,
                gpointer      user_data)
{
        GClueWifiPrivate *priv = GCLUE_WIFI (user_data)->priv;

        if (remove_bss_from_hashtable (path, priv->bss_proxies))
                priv->bss_list_changed = TRUE;
        remove_bss_from_hashtable (path, priv->ignored_bss_proxies);
}

static void
start_wifi_scan (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;
        GVariantBuilder builder;
        GVariant *args;

        g_debug ("Starting WiFi scan…");

        if (priv->scan_done_id == 0)
                priv->scan_done_id = g_signal_connect
                                        (priv->interface,
                                         "scan-done",
                                         G_CALLBACK (on_scan_done),
                                         wifi);

        g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add (&builder,
                               "{sv}",
                               "Type", g_variant_new ("s", "passive"));
        args = g_variant_builder_end (&builder);

        wpa_interface_call_scan (WPA_INTERFACE (priv->interface),
                                 args,
                                 priv->bss_cancellable,
                                 on_scan_call_done,
                                 wifi);
}

static void
cancel_wifi_scan (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;

        if (priv->scan_timeout != 0) {
                g_source_remove (priv->scan_timeout);
                priv->scan_timeout = 0;
        }

        if (priv->scan_wait_id != 0) {
                g_source_remove (priv->scan_wait_id);
                priv->scan_wait_id = 0;
        }

        if (priv->scan_done_id != 0) {
                g_signal_handler_disconnect (priv->interface,
                                             priv->scan_done_id);
                priv->scan_done_id = 0;
        }
}

static gboolean
on_scan_timeout (gpointer user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        GClueWifiPrivate *priv = wifi->priv;

        priv->scan_timeout = 0;

        if (priv->interface == NULL)
                return G_SOURCE_REMOVE;

        start_wifi_scan (wifi);

        return G_SOURCE_REMOVE;
}

gboolean gclue_wifi_should_skip_bsss (GClueAccuracyLevel level)
{
        return level < GCLUE_ACCURACY_LEVEL_STREET;
}

static gboolean
on_scan_wait_done (gpointer wifi)
{
        GClueWifiPrivate *priv;

        g_return_val_if_fail (GCLUE_IS_WIFI (wifi), G_SOURCE_REMOVE);
        priv = GCLUE_WIFI(wifi)->priv;

        /* We have the latest scan result */
        gclue_mozilla_set_wifi (priv->mozilla, wifi);

        if (priv->bss_list_changed) {
                priv->bss_list_changed = FALSE;
                g_debug ("WiFi BSS list changed, refreshing location…");
                gclue_mozilla_set_bss_dirty (priv->mozilla);
                gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));
        }
        priv->scan_wait_id = 0;

        return G_SOURCE_REMOVE;
}

static GClueAccuracyLevel
get_accuracy_level (GClueWifi *wifi)
{
        GClueAccuracyLevel level;

        g_object_get (G_OBJECT (wifi), "accuracy-level", &level, NULL);
        return level;
}

static void
on_scan_done (WPAInterface *object,
              gboolean      success,
              gpointer      user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        GClueWifiPrivate *priv = wifi->priv;
        guint timeout;

        if (!success) {
                g_warning ("WiFi scan failed");

                return;
        }

        if (priv->interface == NULL)
                return;

        if (priv->scan_wait_id != 0)
            g_source_remove (priv->scan_wait_id);

        priv->scan_wait_id = g_timeout_add_seconds (1, on_scan_wait_done, wifi);

        /* If there was another scan already scheduled, cancel that and
         * re-schedule. Regardless of our internal book-keeping, this can happen
         * if wpa_supplicant emits the `ScanDone` signal due to a scan being
         * initiated by another client. */
        if (priv->scan_timeout != 0) {
                g_source_remove (priv->scan_timeout);
                priv->scan_timeout = 0;
        }

        /* With high-enough accuracy requests, we need to scan more often since
         * user's location can change quickly. With low accuracy, we don't since
         * we wouldn't want to drain power unnecessarily.
         */
        if (get_accuracy_level (wifi) >= GCLUE_ACCURACY_LEVEL_STREET)
                timeout = WIFI_SCAN_TIMEOUT_HIGH_ACCURACY;
        else
                timeout = WIFI_SCAN_TIMEOUT_LOW_ACCURACY;
        priv->scan_timeout = g_timeout_add_seconds (timeout,
                                                    on_scan_timeout,
                                                    wifi);
        g_debug ("WiFi scan done, next scheduled in %u seconds", timeout);
}

static void
on_scan_call_done (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
        g_autoptr(GError) error = NULL;

        if (!wpa_interface_call_scan_finish (WPA_INTERFACE (source_object),
                                             res, &error)) {
                GClueWifi *wifi;

                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                        return;
                }

                wifi = GCLUE_WIFI (user_data);

                if (error) {
                        g_warning ("Scanning of WiFi networks failed: %s",
                                   error->message);
                }

                cancel_wifi_scan (wifi);
        }
}

static void
connect_bss_signals (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;
        const gchar *const *bss_list;
        guint i;

        if (priv->bss_added_id != 0)
                return;
        if (priv->interface == NULL) {
                gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));

                return;
        }

        g_assert (!priv->bss_cancellable);
        priv->bss_cancellable = g_cancellable_new ();

        start_wifi_scan (wifi);

        priv->bss_list_changed = TRUE;
        priv->bss_added_id = g_signal_connect (priv->interface,
                                               "bss-added",
                                               G_CALLBACK (on_bss_added),
                                               wifi);
        priv->bss_removed_id = g_signal_connect (priv->interface,
                                                "bss-removed",
                                                G_CALLBACK (on_bss_removed),
                                                wifi);

        bss_list = wpa_interface_get_bsss (WPA_INTERFACE (priv->interface));
        if (bss_list == NULL)
                return;

        for (i = 0; bss_list[i] != NULL; i++)
                on_bss_added (WPA_INTERFACE (priv->interface),
                              bss_list[i],
                              NULL,
                              wifi);
}

static void
disconnect_bss_signals (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;

        if (priv->bss_cancellable) {
                g_debug ("Cancelling WiFi requests");
                g_cancellable_cancel (priv->bss_cancellable);
                g_clear_object (&priv->bss_cancellable);
        }

        cancel_wifi_scan (wifi);

        if (priv->bss_added_id != 0) {
                g_signal_handler_disconnect (priv->interface,
                                             priv->bss_added_id);
                priv->bss_added_id = 0;
        }
        if (priv->bss_removed_id != 0) {
                g_signal_handler_disconnect (priv->interface,
                                             priv->bss_removed_id);
                priv->bss_removed_id = 0;
        }

        g_hash_table_remove_all (priv->bss_proxies);
        g_hash_table_remove_all (priv->ignored_bss_proxies);
}

static void
cache_prune (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;
        GHashTableIter iter;
        gpointer value;
        guint64 cutoff_seconds;
        guint old_cache_size, removed_elements = 0;

        old_cache_size = g_hash_table_size (priv->location_cache);
        cutoff_seconds = g_get_real_time () / G_USEC_PER_SEC - CACHE_ENTRY_MAX_AGE_SECONDS;

        g_hash_table_iter_init (&iter, priv->location_cache);
        while (g_hash_table_iter_next (&iter, NULL, &value)) {
                LocationCacheValue *lcvalue = (LocationCacheValue *)value;
                GList *l = lcvalue->elements;

                g_assert (l);
                while (l) {
                        LocationCacheElement *element = (LocationCacheElement *)l->data;
                        GList *lnext = l->next;

                        /* Keep this location? */
                        if (gclue_location_get_timestamp (element->location) >
                            cutoff_seconds)
                                goto next_el;

                        location_cache_element_free (element);
                        lcvalue->elements = g_list_delete_link (lcvalue->elements, l);
                        removed_elements++;

                        /* Deleted the last entry (element) in this hash bucket?
                         * Remove this hash table entry then.
                         */
                        if (!lcvalue->elements) {
                                g_assert (!lnext);
                                g_hash_table_iter_remove (&iter);
                        }

                next_el:
                        l = lnext;
                }
        }

        g_debug ("Pruned cache (old size: %u, new size: %u, removed elements: %u)",
                 old_cache_size, g_hash_table_size (priv->location_cache),
                 removed_elements);
}

#if GLIB_CHECK_VERSION(2, 64, 0)
static void
cache_empty (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;

        g_debug ("Emptying cache");
        g_hash_table_remove_all (priv->location_cache);
}
#endif  /* GLib ≥ 2.64.0 */

static gboolean
cache_prune_timeout_cb (gpointer user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);

        cache_prune (wifi);

        return G_SOURCE_CONTINUE;
}

#if GLIB_CHECK_VERSION(2, 64, 0)
static void
low_memory_warning_cb (GMemoryMonitor             *memory_monitor,
                       GMemoryMonitorWarningLevel  level,
                       gpointer                    user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);

        if (level == G_MEMORY_MONITOR_WARNING_LEVEL_LOW)
                cache_prune (wifi);
        else if (level > G_MEMORY_MONITOR_WARNING_LEVEL_LOW)
                cache_empty (wifi);
}
#endif  /* GLib ≥ 2.64.0 */

static void
connect_cache_prune_timeout (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;

        g_debug ("Connecting cache prune timeout");

        /* Run the prune at twice the expiry frequency, which should allow us to
         * sample often enough to keep the expiries at that frequency, as per
         * Nyquist. */
        if (priv->cache_prune_timeout_id != 0)
                g_source_remove (priv->cache_prune_timeout_id);
        priv->cache_prune_timeout_id = g_timeout_add_seconds (CACHE_ENTRY_MAX_AGE_SECONDS / 2,
                                                              cache_prune_timeout_cb,
                                                              wifi);

#if GLIB_CHECK_VERSION(2, 64, 0)
        if (priv->memory_monitor == NULL) {
                priv->memory_monitor = g_memory_monitor_dup_default ();
                priv->low_memory_warning_id = g_signal_connect (priv->memory_monitor,
                                                                "low-memory-warning",
                                                                G_CALLBACK (low_memory_warning_cb),
                                                                wifi);
        }
#endif
}

static void
disconnect_cache_prune_timeout (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;

        g_debug ("Disconnecting cache prune timeout");

        /* Run one last prune. */
        cache_prune (wifi);

#if GLIB_CHECK_VERSION(2, 64, 0)
        if (priv->low_memory_warning_id != 0 && priv->memory_monitor != NULL)
                g_signal_handler_disconnect (priv->memory_monitor, priv->low_memory_warning_id);
        g_clear_object (&priv->memory_monitor);
#endif

        if (priv->cache_prune_timeout_id != 0)
                g_source_remove (priv->cache_prune_timeout_id);
        priv->cache_prune_timeout_id = 0;
}

static GClueLocationSourceStartResult
gclue_wifi_start (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;
        GClueLocationSourceStartResult base_result;

        g_return_val_if_fail (GCLUE_IS_WIFI (source),
                              GCLUE_LOCATION_SOURCE_START_RESULT_FAILED);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_wifi_parent_class);
        base_result = base_class->start (source);
        if (base_result != GCLUE_LOCATION_SOURCE_START_RESULT_OK)
                return base_result;

        connect_cache_prune_timeout (GCLUE_WIFI (source));
        connect_bss_signals (GCLUE_WIFI (source));

        return base_result;
}

static GClueLocationSourceStopResult
gclue_wifi_stop (GClueLocationSource *source)
{
        GClueWifi *wifi = GCLUE_WIFI (source);
        GClueWifiPrivate *priv = wifi->priv;
        GClueLocationSourceClass *base_class;
        GClueLocationSourceStopResult base_result;

        g_return_val_if_fail (GCLUE_IS_WIFI (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_wifi_parent_class);
        base_result = base_class->stop (source);
        if (base_result == GCLUE_LOCATION_SOURCE_STOP_RESULT_STILL_USED)
                return base_result;

        disconnect_bss_signals (GCLUE_WIFI (source));
        disconnect_cache_prune_timeout (GCLUE_WIFI (source));

        if (gclue_mozilla_test_set_wifi (priv->mozilla, wifi, NULL)) {
                g_debug ("Removed us as the WiFi source on stop");
        }

        return base_result;
}

static GClueAccuracyLevel
gclue_wifi_get_available_accuracy_level (GClueWebSource *source,
                                         gboolean        net_available)
{
        GClueWifi *wifi = GCLUE_WIFI (source);
        GClueWifiPrivate *priv = wifi->priv;

        if (!net_available)
                return GCLUE_ACCURACY_LEVEL_NONE;
        else if (!priv->interface)
                return GCLUE_ACCURACY_LEVEL_CITY;
        else
                return MIN (get_accuracy_level (wifi), GCLUE_ACCURACY_LEVEL_STREET);
}

static void
on_interface_proxy_ready (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
        GClueWifi *wifi;
        WPAInterface *interface;
        g_autoptr(GError) error = NULL;

        interface = wpa_interface_proxy_new_for_bus_finish (res, &error);
        if (interface == NULL) {
                if (error) {
                        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                                return;
                        }

                        g_warning ("Interface proxy add failed: %s",
                                   error->message);
                }

                return;
        }

        wifi = GCLUE_WIFI (user_data);
        if (wifi->priv->interface != NULL) {
                g_object_unref (interface);
                return;
        }

        wifi->priv->interface = interface;
        g_debug ("WiFi device '%s' added.",
                 wpa_interface_get_ifname (interface));

        if (gclue_location_source_get_active (GCLUE_LOCATION_SOURCE (wifi)))
                connect_bss_signals (wifi);
        else
                gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));
}

static void
on_interface_added (WPASupplicant *supplicant,
                    const gchar   *path,
                    GVariant      *properties,
                    gpointer       user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);

        if (wifi->priv->interface != NULL)
                return;

        wpa_interface_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         "fi.w1.wpa_supplicant1",
                                         path,
                                         wifi->priv->intf_cancellable,
                                         on_interface_proxy_ready,
                                         wifi);
}

static void
on_interface_removed (WPASupplicant *supplicant,
                      const gchar   *path,
                      gpointer       user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        GClueWifiPrivate *priv = wifi->priv;
        const char *object_path;

        if (priv->interface == NULL)
                return;

        object_path = g_dbus_proxy_get_object_path
                        (G_DBUS_PROXY (priv->interface));
        if (g_strcmp0 (object_path, path) != 0)
                return;

        g_debug ("WiFi device '%s' removed.",
                 wpa_interface_get_ifname (priv->interface));

        disconnect_bss_signals (wifi);
        g_clear_object (&wifi->priv->interface);

        if (gclue_mozilla_test_set_wifi (priv->mozilla, wifi, NULL)) {
                g_debug ("Removed interface was the WiFi source");
        }

        gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));
}

static void
gclue_wifi_init (GClueWifi *wifi)
{
        GClueWebSource *web_source = GCLUE_WEB_SOURCE (wifi);

        wifi->priv = gclue_wifi_get_instance_private (wifi);

        wifi->priv->intf_cancellable = g_cancellable_new ();
        wifi->priv->mozilla = gclue_mozilla_get_singleton ();
        gclue_web_source_set_locate_url (web_source,
                                         gclue_mozilla_get_locate_url (wifi->priv->mozilla));
        gclue_web_source_set_submit_url (web_source,
                                         gclue_mozilla_get_submit_url (wifi->priv->mozilla));

        wifi->priv->bss_proxies = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal,
                                                         g_free,
                                                         g_object_unref);
        wifi->priv->ignored_bss_proxies = g_hash_table_new_full (g_str_hash,
                                                                 g_str_equal,
                                                                 g_free,
                                                                 g_object_unref);
        wifi->priv->location_cache = g_hash_table_new_full (variant_hash,
                                                            g_variant_equal,
                                                            (GDestroyNotify) g_variant_unref,
                                                            location_cache_value_free);
}

static void
gclue_wifi_constructed (GObject *object)
{
        GClueWifi *wifi = GCLUE_WIFI (object);
        GClueWifiPrivate *priv = wifi->priv;
        const gchar *const *interfaces;
        g_autoptr(GError) error = NULL;

        G_OBJECT_CLASS (gclue_wifi_parent_class)->constructed (object);

        if (get_accuracy_level (wifi) == GCLUE_ACCURACY_LEVEL_CITY) {
                GClueConfig *config = gclue_config_get_singleton ();

                if (!gclue_config_get_enable_wifi_source (config))
                        goto refresh_n_exit;
        }

        /* FIXME: We should be using async variant */
        priv->supplicant = wpa_supplicant_proxy_new_for_bus_sync
                        (G_BUS_TYPE_SYSTEM,
                         G_DBUS_PROXY_FLAGS_NONE,
                         "fi.w1.wpa_supplicant1",
                         "/fi/w1/wpa_supplicant1",
                         NULL,
                         &error);
        if (priv->supplicant == NULL) {
                if (error)
                        g_warning ("Failed to connect to wpa_supplicant service: %s",
                                   error->message);
                goto refresh_n_exit;
        }

        g_signal_connect_object (priv->supplicant,
                                 "interface-added",
                                 G_CALLBACK (on_interface_added),
                                 wifi, 0);
        g_signal_connect_object (priv->supplicant,
                                 "interface-removed",
                                 G_CALLBACK (on_interface_removed),
                                 wifi, 0);

        interfaces = wpa_supplicant_get_interfaces (priv->supplicant);
        if (interfaces != NULL && interfaces[0] != NULL)
                on_interface_added (priv->supplicant,
                                    interfaces[0],
                                    NULL,
                                    wifi);

refresh_n_exit:
        gclue_web_source_refresh (GCLUE_WEB_SOURCE (object));
}

static void
on_wifi_destroyed (gpointer data,
                   GObject *where_the_object_was)
{
        GClueWifi **wifi = (GClueWifi **) data;

        *wifi = NULL;
}

/**
 * gclue_wifi_new:
 *
 * Get the #GClueWifi singleton, for the specified max accuracy level @level.
 *
 * Returns: (transfer full): a new ref to #GClueWifi. Use g_object_unref()
 * when done.
 **/
GClueWifi *
gclue_wifi_get_singleton (GClueAccuracyLevel level)
{
        static GClueWifi *wifi[] = { NULL, NULL, NULL };
        guint i;
        GClueConfig *config = gclue_config_get_singleton ();
        gboolean wifi_enabled;
        gboolean scramble_location = FALSE;
        gboolean compute_movement = FALSE;

        g_return_val_if_fail (level >= GCLUE_ACCURACY_LEVEL_CITY, NULL);

        wifi_enabled = gclue_config_get_enable_wifi_source (config);
        if (level == GCLUE_ACCURACY_LEVEL_CITY) {
                i = 0;
                if (wifi_enabled)
                        scramble_location = TRUE;
        } else if (level == GCLUE_ACCURACY_LEVEL_NEIGHBORHOOD) {
                g_return_val_if_fail (wifi_enabled, NULL);

                i = 1;
                scramble_location = TRUE;
        } else {
                g_return_val_if_fail (wifi_enabled, NULL);

                i = 2;
                compute_movement = TRUE;
        }

        if (wifi[i] == NULL) {
                wifi[i] = g_object_new (GCLUE_TYPE_WIFI,
                                        "accuracy-level", level,
                                        "scramble-location", scramble_location,
                                        "compute-movement", compute_movement,
                                        NULL);
                g_object_weak_ref (G_OBJECT (wifi[i]),
                                   on_wifi_destroyed,
                                   &wifi[i]);
        } else
                g_object_ref (wifi[i]);

        return wifi[i];
}

static gboolean
wifi_should_skip_tower (GClueWifi *wifi)
{
        return gclue_3g_should_skip_tower (get_accuracy_level (wifi));
}

/* Can return NULL, signifying an empty BSS list. */
GList *
gclue_wifi_get_bss_list (GClueWifi *wifi)
{
        return g_hash_table_get_values (wifi->priv->bss_proxies);
}

static SoupMessage *
gclue_wifi_create_query (GClueWebSource *source,
                         const char **query_data_description,
                         GError        **error)
{
        GClueWifi *wifi = GCLUE_WIFI (source);
        gboolean skip_tower;

        if (wifi->priv->interface == NULL) {
                goto create_query;
        }

        /* Empty list? */
        if (!g_hash_table_size (wifi->priv->bss_proxies)) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "No WiFi networks found");
                return NULL;
        }

create_query:
        skip_tower = wifi_should_skip_tower (wifi);
        if (skip_tower) {
                g_debug ("Will skip 3GPP tower in query due to our accuracy level");
        }

        return gclue_mozilla_create_query (wifi->priv->mozilla, skip_tower, FALSE,
                                           query_data_description, error);
}

static SoupMessage *
gclue_wifi_create_submit_query (GClueWebSource  *source,
                                GClueLocation   *location,
                                GError         **error)
{
        GClueWifi *wifi = GCLUE_WIFI (source);
        SoupMessage * msg;

        if (wifi->priv->interface == NULL) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "No WiFi devices available");
                return NULL;
        }

        /* Empty list? */
        if (!g_hash_table_size (wifi->priv->bss_proxies)) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "No WiFi networks found");
                return NULL;
        }

        msg = gclue_mozilla_create_submit_query (wifi->priv->mozilla,
                                                 location,
                                                 error);
        return msg;
}

static void refresh_cb (GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      user_data);

static gint
bss_compare (gconstpointer a,
             gconstpointer b)
{
        WPABSS **_bss_a = (WPABSS **) a;
        WPABSS **_bss_b = (WPABSS **) b;
        WPABSS *bss_a = WPA_BSS (*_bss_a);
        WPABSS *bss_b = WPA_BSS (*_bss_b);
        GVariant *bssid_a = wpa_bss_get_bssid (bss_a);
        GVariant *bssid_b = wpa_bss_get_bssid (bss_b);
        g_autoptr(GBytes) bssid_bytes_a = NULL;
        g_autoptr(GBytes) bssid_bytes_b = NULL;

        /* Can’t use g_variant_compare() as it isn’t defined over `ay` types */
        if (bssid_a == NULL && bssid_b == NULL)
                return 0;
        else if (bssid_a == NULL)
                return -1;
        else if (bssid_b == NULL)
                return 1;

        bssid_bytes_a = g_variant_get_data_as_bytes (bssid_a);
        bssid_bytes_b = g_variant_get_data_as_bytes (bssid_b);

        return g_bytes_compare (bssid_bytes_a, bssid_bytes_b);
}

static guint
variant_hash (gconstpointer key)
{
        GVariant *variant = (GVariant *) key;
        g_autoptr(GBytes) bytes = g_variant_get_data_as_bytes (variant);
        return g_bytes_hash (bytes);
}

static void location_cache_key_fill_tower (GClueWifi *wifi, GClue3GTower *tower)
{
        GClueWifiPrivate *priv = wifi->priv;
        GClue3GTower *moztower;

        memset (tower, 0, sizeof (*tower));
        tower->tec = GCLUE_TOWER_TEC_NO_FIX;

        moztower = gclue_mozilla_get_tower (priv->mozilla);
        if (!moztower || wifi_should_skip_tower (wifi)) {
                return;
        }

        g_assert (moztower->tec != GCLUE_TOWER_TEC_NO_FIX);
        *tower = *moztower;
}

static void location_cache_key_add_tower (GClueWifi *wifi, GVariantBuilder *builder)
{
        GClue3GTower tower;

        location_cache_key_fill_tower (wifi, &tower);
        g_variant_builder_add (builder, "u", (guint32)tower.tec);
        g_variant_builder_add (builder, "s", tower.opc);
        g_variant_builder_add (builder, "t", (guint64)tower.lac);
        g_variant_builder_add (builder, "t", (guint64)tower.cell_id);
}

static GPtrArray *
get_location_cache_bss_array (GClueWifi *wifi)
{
        GHashTableIter iter;
        gpointer value;
        g_autoptr(GPtrArray) bss_array = g_ptr_array_new_with_free_func (NULL);  /* (element-type WPABSS) */

        /* The Mozilla service puts BSSID and signal strength for each BSS into
         * its query. Pack the whole lot into a #GVariant for simplicity, sorted
         * by MAC address. The sorting has to happen in an array beforehand,
         * as variants are immutable.
         */
        g_hash_table_iter_init (&iter, wifi->priv->bss_proxies);

        while (g_hash_table_iter_next (&iter, NULL, &value)) {
                WPABSS *bss = WPA_BSS (value);
                if (bss != NULL)
                        g_ptr_array_add (bss_array, bss);
        }

        g_ptr_array_sort (bss_array, bss_compare);

        return g_steal_pointer (&bss_array);
}

static GVariant *
get_location_cache_hashtable_key (GClueWifi *wifi, GPtrArray *bss_array)
{
        guint i;
        GVariantBuilder builder;

        /* Serialise to a variant. */
        g_variant_builder_init (&builder, G_VARIANT_TYPE ("(usttaay)"));
        location_cache_key_add_tower (wifi, &builder);

        g_variant_builder_open (&builder, G_VARIANT_TYPE ("aay"));
        for (i = 0; i < bss_array->len; i++) {
                WPABSS *bss = WPA_BSS (bss_array->pdata[i]);
                GVariant *bssid;

                bssid = wpa_bss_get_bssid (bss);
                if (bssid == NULL)
                        continue;

                g_variant_builder_add_value (&builder, bssid);
        }
        g_variant_builder_close (&builder);

        return g_variant_builder_end (&builder);
}

static GArray *
get_location_cache_signal_array (GClueWifi *wifi, GPtrArray *bss_array)
{
        g_autoptr(GArray) signal_array = NULL;
        guint i;

        signal_array = g_array_sized_new (FALSE, FALSE, sizeof (gint16), bss_array->len);
        for (i = 0; i < bss_array->len; i++) {
                WPABSS *bss = WPA_BSS (bss_array->pdata[i]);
                gint16 signal = wpa_bss_get_signal (bss);

                g_array_append_val (signal_array, signal);
        }

        return g_steal_pointer (&signal_array);
}

static gboolean cached_signals_match (GArray *signals1, GArray *signals2)
{
        guint i;

        if (signals1->len != signals2->len) {
                g_warning ("Different signal count in one hash table entry: %u vs %u",
                           signals1->len, signals2->len);
                return FALSE;
        }

        for (i = 0; i < signals1->len; i++) {
                gint s1 = g_array_index (signals1, gint16, i);
                gint s2 = g_array_index (signals2, gint16, i);

                if (ABS (s1 - s2) > CACHE_ENTRY_MATCH_SIGNAL_WINDOW / 2)
                        return FALSE;
        }

        return TRUE;
}

static GClueLocation *
find_cached_location (GHashTable *cache, GVariant *key, GArray *signals)
{
        g_autofree gchar *key_str = g_variant_print (key, FALSE);
        GClueLocation *location = NULL;
        LocationCacheValue *value;
        GList *l;

        value = g_hash_table_lookup (cache, key);
        if (!value) {
                g_debug ("Cache miss for key %s", key_str);
                return NULL;
        }

        g_assert (value->elements);
        for (l = value->elements; l; l = l->next) {
                LocationCacheElement *element = l->data;

                if (location &&
                    gclue_location_get_accuracy (element->location) >=
                    gclue_location_get_accuracy (location)) {
                        /* Have at least as accurate location already,
                         * don't bother with comparing signals.
                         */
                        continue;
                }

                if (!cached_signals_match (element->signals, signals))
                        continue;

                location = element->location;
        }

        if (location) {
                g_debug ("Cache hit for key %s: got location %p (%s)",
                         key_str, location,
                         gclue_location_get_description (location));
        } else {
                g_debug ("Cache had key %s, but with different signals", key_str);
        }

        return location;
}

typedef struct {
        GVariant *cache_key;
        GArray *signals;
} RefreshTaskData;

static RefreshTaskData *
refresh_task_data_new (GVariant *cache_key,
                       GArray *signals)
{
        RefreshTaskData *tdata;

        tdata = g_slice_new (RefreshTaskData);
        tdata->cache_key = g_variant_ref (cache_key);
        tdata->signals = signals;
        return tdata;
}

static void refresh_task_data_free (gpointer data)
{
        RefreshTaskData *rdata = data;

        g_clear_pointer (&rdata->cache_key, g_variant_unref);
        if (rdata->signals)
                g_array_free (rdata->signals, TRUE);
        g_slice_free (RefreshTaskData, rdata);
}

static void
gclue_wifi_refresh_async (GClueWebSource      *source,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (source);
        g_autoptr(GTask) task = g_task_new (source, cancellable, callback, user_data);
        g_autoptr(GPtrArray) bss_array = get_location_cache_bss_array (wifi);
        g_autoptr(GVariant) cache_key = get_location_cache_hashtable_key (wifi, bss_array);
        g_autoptr(GArray) signal_array = get_location_cache_signal_array (wifi, bss_array);
        GClueLocation *cached_location = find_cached_location (wifi->priv->location_cache,
                                                               cache_key, signal_array);
        RefreshTaskData *tdata;

        g_task_set_source_tag (task, gclue_wifi_refresh_async);

        if (gclue_location_source_get_active (GCLUE_LOCATION_SOURCE (source))) {
                /* Try the cache. */
                if (cached_location != NULL) {
                        g_autoptr(GClueLocation) new_location = NULL;

                        wifi->priv->cache_hits++;

                        /* Duplicate the location so its timestamp is updated. */
                        new_location = gclue_location_duplicate_fresh (cached_location);
                        gclue_location_source_set_location (GCLUE_LOCATION_SOURCE (source), new_location);

                        g_task_return_pointer (task, g_steal_pointer (&new_location), g_object_unref);
                        return;
                }

                wifi->priv->cache_misses++;
        }

        tdata = refresh_task_data_new (cache_key, g_steal_pointer (&signal_array));
        g_task_set_task_data (task, tdata, refresh_task_data_free);

        /* Fall back to querying the web service. */
        GCLUE_WEB_SOURCE_CLASS (gclue_wifi_parent_class)->refresh_async (source, cancellable, refresh_cb, g_steal_pointer (&task));
}

static void
add_cached_location (GHashTable *cache,
                     GVariant *key, GArray **signals,
                     GClueLocation *location)
{
        LocationCacheValue *value;
        LocationCacheElement *element;

        value = g_hash_table_lookup (cache, key);
        if (!value) {
                value = location_cache_value_new ();
                g_hash_table_insert (cache, g_variant_ref (key), value);
        }

        element = location_cache_element_new (g_steal_pointer (signals), location);
        value->elements = g_list_prepend (value->elements, element);
}

static void
refresh_cb (GObject      *source_object,
            GAsyncResult *result,
            gpointer      user_data)
{
        GClueWebSource *source = GCLUE_WEB_SOURCE (source_object);
        GClueWifi *wifi = GCLUE_WIFI (source);
        g_autoptr(GTask) task = g_steal_pointer (&user_data);
        g_autoptr(GClueLocation) location = NULL;
        g_autoptr(GError) local_error = NULL;
        RefreshTaskData *tdata;
        g_autofree gchar *cache_key_str = NULL;
        double cache_hit_ratio;

        /* Finish querying the web service. */
        location = GCLUE_WEB_SOURCE_CLASS (gclue_wifi_parent_class)->refresh_finish (source, result, &local_error);

        if (local_error != NULL) {
                g_task_return_error (task, g_steal_pointer (&local_error));
                return;
        }

        /* Cache the result. */
        tdata = g_task_get_task_data (task);
        cache_key_str = g_variant_print (tdata->cache_key, FALSE);
        add_cached_location (wifi->priv->location_cache,
                             tdata->cache_key, &tdata->signals,
                             location);

        if (wifi->priv->cache_hits || wifi->priv->cache_misses) {
                double cache_attempts;

                cache_attempts = wifi->priv->cache_hits;
                cache_attempts += wifi->priv->cache_misses;
                cache_hit_ratio = wifi->priv->cache_hits * 100.0 / cache_attempts;
        } else {
                cache_hit_ratio = 0;
        }

        g_debug ("Adding %s / %s to cache (new size: %u; hit ratio %.2f%%)",
                 cache_key_str,
                 gclue_location_get_description (location),
                 g_hash_table_size (wifi->priv->location_cache),
                 cache_hit_ratio);

        g_task_return_pointer (task, g_steal_pointer (&location), g_object_unref);
}

static GClueLocation *
gclue_wifi_refresh_finish (GClueWebSource  *source,
                           GAsyncResult    *result,
                           GError         **error)
{
        GTask *task = G_TASK (result);

        return g_task_propagate_pointer (task, error);
}
