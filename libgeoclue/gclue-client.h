/*
 * Generated by gdbus-codegen 2.54.3. DO NOT EDIT.
 *
 * The license of this code is the same as for the source it was derived from.
 */

#ifndef __GCLUE_CLIENT_H__
#define __GCLUE_CLIENT_H__

#include <gio/gio.h>

G_BEGIN_DECLS


/* ------------------------------------------------------------------------ */
/* Declarations for org.freedesktop.GeoClue2.Client */

#define GCLUE_TYPE_CLIENT (gclue_client_get_type ())
#define GCLUE_CLIENT(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GCLUE_TYPE_CLIENT, GClueClient))
#define GCLUE_IS_CLIENT(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCLUE_TYPE_CLIENT))
#define GCLUE_CLIENT_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), GCLUE_TYPE_CLIENT, GClueClientIface))

struct _GClueClient;
typedef struct _GClueClient GClueClient;
typedef struct _GClueClientIface GClueClientIface;

struct _GClueClientIface
{
  GTypeInterface parent_iface;



  gboolean (*handle_start) (
    GClueClient *object,
    GDBusMethodInvocation *invocation);

  gboolean (*handle_stop) (
    GClueClient *object,
    GDBusMethodInvocation *invocation);

  gboolean  (*get_active) (GClueClient *object);

  const gchar * (*get_desktop_id) (GClueClient *object);

  guint  (*get_distance_threshold) (GClueClient *object);

  const gchar * (*get_location) (GClueClient *object);

  guint  (*get_requested_accuracy_level) (GClueClient *object);

  guint  (*get_time_threshold) (GClueClient *object);

  void (*location_updated) (
    GClueClient *object,
    const gchar *arg_old,
    const gchar *arg_new);

};

GType gclue_client_get_type (void) G_GNUC_CONST;

GDBusInterfaceInfo *gclue_client_interface_info (void);
guint gclue_client_override_properties (GObjectClass *klass, guint property_id_begin);


/* D-Bus method call completion functions: */
void gclue_client_complete_start (
    GClueClient *object,
    GDBusMethodInvocation *invocation);

void gclue_client_complete_stop (
    GClueClient *object,
    GDBusMethodInvocation *invocation);



/* D-Bus signal emissions functions: */
void gclue_client_emit_location_updated (
    GClueClient *object,
    const gchar *arg_old,
    const gchar *arg_new);



/* D-Bus method calls: */
void gclue_client_call_start (
    GClueClient *proxy,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean gclue_client_call_start_finish (
    GClueClient *proxy,
    GAsyncResult *res,
    GError **error);

gboolean gclue_client_call_start_sync (
    GClueClient *proxy,
    GCancellable *cancellable,
    GError **error);

void gclue_client_call_stop (
    GClueClient *proxy,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean gclue_client_call_stop_finish (
    GClueClient *proxy,
    GAsyncResult *res,
    GError **error);

gboolean gclue_client_call_stop_sync (
    GClueClient *proxy,
    GCancellable *cancellable,
    GError **error);



/* D-Bus property accessors: */
const gchar *gclue_client_get_location (GClueClient *object);
gchar *gclue_client_dup_location (GClueClient *object);
void gclue_client_set_location (GClueClient *object, const gchar *value);

guint gclue_client_get_distance_threshold (GClueClient *object);
void gclue_client_set_distance_threshold (GClueClient *object, guint value);

guint gclue_client_get_time_threshold (GClueClient *object);
void gclue_client_set_time_threshold (GClueClient *object, guint value);

const gchar *gclue_client_get_desktop_id (GClueClient *object);
gchar *gclue_client_dup_desktop_id (GClueClient *object);
void gclue_client_set_desktop_id (GClueClient *object, const gchar *value);

guint gclue_client_get_requested_accuracy_level (GClueClient *object);
void gclue_client_set_requested_accuracy_level (GClueClient *object, guint value);

gboolean gclue_client_get_active (GClueClient *object);
void gclue_client_set_active (GClueClient *object, gboolean value);


/* ---- */

#define GCLUE_TYPE_CLIENT_PROXY (gclue_client_proxy_get_type ())
#define GCLUE_CLIENT_PROXY(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GCLUE_TYPE_CLIENT_PROXY, GClueClientProxy))
#define GCLUE_CLIENT_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GCLUE_TYPE_CLIENT_PROXY, GClueClientProxyClass))
#define GCLUE_CLIENT_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GCLUE_TYPE_CLIENT_PROXY, GClueClientProxyClass))
#define GCLUE_IS_CLIENT_PROXY(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCLUE_TYPE_CLIENT_PROXY))
#define GCLUE_IS_CLIENT_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GCLUE_TYPE_CLIENT_PROXY))

typedef struct _GClueClientProxy GClueClientProxy;
typedef struct _GClueClientProxyClass GClueClientProxyClass;
typedef struct _GClueClientProxyPrivate GClueClientProxyPrivate;

struct _GClueClientProxy
{
  /*< private >*/
  GDBusProxy parent_instance;
  GClueClientProxyPrivate *priv;
};

struct _GClueClientProxyClass
{
  GDBusProxyClass parent_class;
};

GType gclue_client_proxy_get_type (void) G_GNUC_CONST;

#if GLIB_CHECK_VERSION(2, 44, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GClueClientProxy, g_object_unref)
#endif

void gclue_client_proxy_new (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
GClueClient *gclue_client_proxy_new_finish (
    GAsyncResult        *res,
    GError             **error);
GClueClient *gclue_client_proxy_new_sync (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);

void gclue_client_proxy_new_for_bus (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
GClueClient *gclue_client_proxy_new_for_bus_finish (
    GAsyncResult        *res,
    GError             **error);
GClueClient *gclue_client_proxy_new_for_bus_sync (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);


/* ---- */

#define GCLUE_TYPE_CLIENT_SKELETON (gclue_client_skeleton_get_type ())
#define GCLUE_CLIENT_SKELETON(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GCLUE_TYPE_CLIENT_SKELETON, GClueClientSkeleton))
#define GCLUE_CLIENT_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GCLUE_TYPE_CLIENT_SKELETON, GClueClientSkeletonClass))
#define GCLUE_CLIENT_SKELETON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GCLUE_TYPE_CLIENT_SKELETON, GClueClientSkeletonClass))
#define GCLUE_IS_CLIENT_SKELETON(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCLUE_TYPE_CLIENT_SKELETON))
#define GCLUE_IS_CLIENT_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GCLUE_TYPE_CLIENT_SKELETON))

typedef struct _GClueClientSkeleton GClueClientSkeleton;
typedef struct _GClueClientSkeletonClass GClueClientSkeletonClass;
typedef struct _GClueClientSkeletonPrivate GClueClientSkeletonPrivate;

struct _GClueClientSkeleton
{
  /*< private >*/
  GDBusInterfaceSkeleton parent_instance;
  GClueClientSkeletonPrivate *priv;
};

struct _GClueClientSkeletonClass
{
  GDBusInterfaceSkeletonClass parent_class;
};

GType gclue_client_skeleton_get_type (void) G_GNUC_CONST;

#if GLIB_CHECK_VERSION(2, 44, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GClueClientSkeleton, g_object_unref)
#endif

GClueClient *gclue_client_skeleton_new (void);


G_END_DECLS

#endif /* __GCLUE_CLIENT_H__ */
