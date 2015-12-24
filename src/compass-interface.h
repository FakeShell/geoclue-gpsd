/*
 * Generated by gdbus-codegen 2.47.3. DO NOT EDIT.
 *
 * The license of this code is the same as for the source it was derived from.
 */

#ifndef __COMPASS_INTERFACE_H__
#define __COMPASS_INTERFACE_H__

#include <gio/gio.h>

G_BEGIN_DECLS


/* ------------------------------------------------------------------------ */
/* Declarations for net.hadess.SensorProxy.Compass */

#define TYPE_COMPASS (compass_get_type ())
#define COMPASS(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_COMPASS, Compass))
#define IS_COMPASS(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_COMPASS))
#define COMPASS_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), TYPE_COMPASS, CompassIface))

struct _Compass;
typedef struct _Compass Compass;
typedef struct _CompassIface CompassIface;

struct _CompassIface
{
  GTypeInterface parent_iface;


  gboolean (*handle_claim_compass) (
    Compass *object,
    GDBusMethodInvocation *invocation);

  gboolean (*handle_release_compass) (
    Compass *object,
    GDBusMethodInvocation *invocation);

  gdouble  (*get_compass_heading) (Compass *object);

  gboolean  (*get_has_compass) (Compass *object);

};

GType compass_get_type (void) G_GNUC_CONST;

GDBusInterfaceInfo *compass_interface_info (void);
guint compass_override_properties (GObjectClass *klass, guint property_id_begin);


/* D-Bus method call completion functions: */
void compass_complete_claim_compass (
    Compass *object,
    GDBusMethodInvocation *invocation);

void compass_complete_release_compass (
    Compass *object,
    GDBusMethodInvocation *invocation);



/* D-Bus method calls: */
void compass_call_claim_compass (
    Compass *proxy,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean compass_call_claim_compass_finish (
    Compass *proxy,
    GAsyncResult *res,
    GError **error);

gboolean compass_call_claim_compass_sync (
    Compass *proxy,
    GCancellable *cancellable,
    GError **error);

void compass_call_release_compass (
    Compass *proxy,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean compass_call_release_compass_finish (
    Compass *proxy,
    GAsyncResult *res,
    GError **error);

gboolean compass_call_release_compass_sync (
    Compass *proxy,
    GCancellable *cancellable,
    GError **error);



/* D-Bus property accessors: */
gboolean compass_get_has_compass (Compass *object);
void compass_set_has_compass (Compass *object, gboolean value);

gdouble compass_get_compass_heading (Compass *object);
void compass_set_compass_heading (Compass *object, gdouble value);


/* ---- */

#define TYPE_COMPASS_PROXY (compass_proxy_get_type ())
#define COMPASS_PROXY(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_COMPASS_PROXY, CompassProxy))
#define COMPASS_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), TYPE_COMPASS_PROXY, CompassProxyClass))
#define COMPASS_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_COMPASS_PROXY, CompassProxyClass))
#define IS_COMPASS_PROXY(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_COMPASS_PROXY))
#define IS_COMPASS_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_COMPASS_PROXY))

typedef struct _CompassProxy CompassProxy;
typedef struct _CompassProxyClass CompassProxyClass;
typedef struct _CompassProxyPrivate CompassProxyPrivate;

struct _CompassProxy
{
  /*< private >*/
  GDBusProxy parent_instance;
  CompassProxyPrivate *priv;
};

struct _CompassProxyClass
{
  GDBusProxyClass parent_class;
};

GType compass_proxy_get_type (void) G_GNUC_CONST;

void compass_proxy_new (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
Compass *compass_proxy_new_finish (
    GAsyncResult        *res,
    GError             **error);
Compass *compass_proxy_new_sync (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);

void compass_proxy_new_for_bus (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
Compass *compass_proxy_new_for_bus_finish (
    GAsyncResult        *res,
    GError             **error);
Compass *compass_proxy_new_for_bus_sync (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);


/* ---- */

#define TYPE_COMPASS_SKELETON (compass_skeleton_get_type ())
#define COMPASS_SKELETON(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_COMPASS_SKELETON, CompassSkeleton))
#define COMPASS_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), TYPE_COMPASS_SKELETON, CompassSkeletonClass))
#define COMPASS_SKELETON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_COMPASS_SKELETON, CompassSkeletonClass))
#define IS_COMPASS_SKELETON(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_COMPASS_SKELETON))
#define IS_COMPASS_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_COMPASS_SKELETON))

typedef struct _CompassSkeleton CompassSkeleton;
typedef struct _CompassSkeletonClass CompassSkeletonClass;
typedef struct _CompassSkeletonPrivate CompassSkeletonPrivate;

struct _CompassSkeleton
{
  /*< private >*/
  GDBusInterfaceSkeleton parent_instance;
  CompassSkeletonPrivate *priv;
};

struct _CompassSkeletonClass
{
  GDBusInterfaceSkeletonClass parent_class;
};

GType compass_skeleton_get_type (void) G_GNUC_CONST;

Compass *compass_skeleton_new (void);


G_END_DECLS

#endif /* __COMPASS_INTERFACE_H__ */
