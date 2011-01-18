/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:cd-client
 * @short_description: Main client object for accessing the colord daemon
 *
 * A helper GObject to use for accessing colord information, and to be notified
 * when it is changed.
 *
 * See also: #CdDevice
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

#include "cd-client.h"
#include "cd-device.h"

static void	cd_client_class_init	(CdClientClass	*klass);
static void	cd_client_init		(CdClient	*client);
static void	cd_client_finalize	(GObject	*object);

#define CD_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_CLIENT, CdClientPrivate))

/**
 * CdClientPrivate:
 *
 * Private #CdClient data
 **/
struct _CdClientPrivate
{
	GDBusProxy		*proxy;
	gchar			*daemon_version;
};

enum {
	CD_CLIENT_CHANGED,
	CD_CLIENT_DEVICE_ADDED,
	CD_CLIENT_DEVICE_REMOVED,
	CD_CLIENT_LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_DAEMON_VERSION,
	PROP_LAST
};

static guint signals [CD_CLIENT_LAST_SIGNAL] = { 0 };
static gpointer cd_client_object = NULL;

G_DEFINE_TYPE (CdClient, cd_client, G_TYPE_OBJECT)

/**
 * cd_client_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.0
 **/
GQuark
cd_client_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("cd_client_error");
	return quark;
}

/**
 * cd_client_get_device_array_from_variant:
 **/
static GPtrArray *
cd_client_get_device_array_from_variant (GVariant *result,
					 GCancellable *cancellable,
					 GError **error)
{
	CdDevice *device;
	gboolean ret;
	gchar *object_path_tmp;
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each device */
	array_tmp = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		g_debug ("%s", object_path_tmp);

		/* create device and add to the array */
		device = cd_device_new ();
		ret = cd_device_set_object_path_sync (device,
						      object_path_tmp,
						      cancellable,
						      &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_FAILED,
				     "Failed to set device object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (device);
			goto out;
		}
		g_ptr_array_add (array_tmp, device);
		g_free (object_path_tmp);
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	if (child != NULL)
		g_variant_unref (child);
	if (array_tmp != NULL)
		g_ptr_array_unref (array_tmp);
	return array;
}

/**
 * cd_client_get_devices_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Get an array of the device objects.
 *
 * Return value: (transfer full): an array of #CdDevice objects,
 *		 free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_client_get_devices_sync (CdClient *client,
			    GCancellable *cancellable,
			    GError **error)
{
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "GetDevices",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to GetDevices: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* convert to array of CdDevice's */
	array = cd_client_get_device_array_from_variant (result,
							 cancellable,
							 error);
	if (array == NULL)
		goto out;
out:
	if (result != NULL)
		g_variant_unref (result);
	return array;
}

/**
 * cd_client_create_device_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Creates a color device.
 *
 * Return value: A #CdDevice object, or %NULL for error
 *
 * Since: 0.1.0
 **/
CdDevice *
cd_client_create_device_sync (CdClient *client,
			      const gchar *id,
			      guint options,
			      GCancellable *cancellable,
			      GError **error)
{
	CdDevice *device = NULL;
	CdDevice *device_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "CreateDevice",
					 g_variant_new ("(su)",
						        id,
						        options),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to CreateDevice: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create thick CdDevice object */
	g_variant_get (result, "(o)",
		       &object_path);
	device_tmp = cd_device_new ();
	ret = cd_device_set_object_path_sync (device_tmp,
					      object_path,
					      cancellable,
					      error);
	if (!ret)
		goto out;

	/* success */
	device = g_object_ref (device_tmp);
out:
	g_free (object_path);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	if (result != NULL)
		g_variant_unref (result);
	return device;
}

/**
 * cd_client_create_profile_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Creates a color profile.
 *
 * Return value: A #CdProfile object, or %NULL for error
 *
 * Since: 0.1.0
 **/
CdProfile *
cd_client_create_profile_sync (CdClient *client,
			       const gchar *id,
			       guint options,
			       GCancellable *cancellable,
			       GError **error)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "CreateProfile",
					 g_variant_new ("(su)",
						        id,
						        options),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to CreateProfile: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create thick CdDevice object */
	g_variant_get (result, "(o)",
		       &object_path);
	profile_tmp = cd_profile_new ();
	ret = cd_profile_set_object_path_sync (profile_tmp,
					       object_path,
					       cancellable,
					       error);
	if (!ret)
		goto out;

	/* success */
	profile = g_object_ref (profile_tmp);
out:
	g_free (object_path);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	if (result != NULL)
		g_variant_unref (result);
	return profile;
}

/**
 * cd_client_delete_device_sync:
 * @client: a #CdClient instance.
 * @device: a #CdDevice instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Deletes a color device.
 *
 * Return value: %TRUE is the device was deleted
 *
 * Since: 0.1.0
 **/
gboolean
cd_client_delete_device_sync (CdClient *client,
			      const gchar *id,
			      GCancellable *cancellable,
			      GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy != NULL, FALSE);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "DeleteDevice",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to DeleteDevice: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * cd_client_delete_profile_sync:
 * @client: a #CdClient instance.
 * @profile: a #CdProfile instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Deletes a color profile.
 *
 * Return value: %TRUE is the profile was deleted
 *
 * Since: 0.1.0
 **/
gboolean
cd_client_delete_profile_sync (CdClient *client,
			       const gchar *id,
			       GCancellable *cancellable,
			       GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy != NULL, FALSE);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "DeleteProfile",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to DeleteProfile: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (result != NULL)
		g_variant_unref (result);
	return ret;
}

/**
 * cd_client_find_device_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Finds a color device.
 *
 * Return value: A #CdDevice object, or %NULL for error
 *
 * Since: 0.1.0
 **/
CdDevice *
cd_client_find_device_sync (CdClient *client,
			    const gchar *id,
			    GCancellable *cancellable,
			    GError **error)
{
	CdDevice *device = NULL;
	CdDevice *device_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "FindDeviceById",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to FindDeviceById: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create GObject CdDevice object */
	g_variant_get (result, "(o)",
		       &object_path);
	device_tmp = cd_device_new ();
	ret = cd_device_set_object_path_sync (device_tmp,
					      object_path,
					      cancellable,
					      error);
	if (!ret)
		goto out;

	/* success */
	device = g_object_ref (device_tmp);
out:
	g_free (object_path);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	if (result != NULL)
		g_variant_unref (result);
	return device;
}

/**
 * cd_client_find_profile_sync:
 * @client: a #CdClient instance.
 * @id: identifier for the device
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Finds a color profile.
 *
 * Return value: A #CdProfile object, or %NULL for error
 *
 * Since: 0.1.0
 **/
CdProfile *
cd_client_find_profile_sync (CdClient *client,
			     const gchar *id,
			     GCancellable *cancellable,
			     GError **error)
{
	CdProfile *profile = NULL;
	CdProfile *profile_tmp = NULL;
	gboolean ret;
	gchar *object_path = NULL;
	GError *error_local = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "FindProfileById",
					 g_variant_new ("(s)", id),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to FindProfileById: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* create GObject CdDevice object */
	g_variant_get (result, "(o)",
		       &object_path);
	profile_tmp = cd_profile_new ();
	ret = cd_profile_set_object_path_sync (profile_tmp,
					       object_path,
					       cancellable,
					       error);
	if (!ret)
		goto out;

	/* success */
	profile = g_object_ref (profile_tmp);
out:
	g_free (object_path);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	if (result != NULL)
		g_variant_unref (result);
	return profile;
}

/**
 * cd_client_get_devices_by_kind_sync:
 * @client: a #CdClient instance.
 * @kind: a #CdDeviceKind, e.g. %CD_DEVICE_KIND_DISPLAY
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Get an array of the device objects of a specified kind.
 *
 * Return value: (transfer full): an array of #CdDevice objects,
 *		 free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_client_get_devices_by_kind_sync (CdClient *client,
				    CdDeviceKind kind,
				    GCancellable *cancellable,
				    GError **error)
{
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "GetDevicesByKind",
					 g_variant_new ("(s)",
					 	        cd_device_kind_to_string (kind)),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to GetDevicesByKind: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* convert to array of CdDevice's */
	array = cd_client_get_device_array_from_variant (result,
							 cancellable,
							 error);
	if (array == NULL)
		goto out;
out:
	if (result != NULL)
		g_variant_unref (result);
	return array;
}

/**
 * cd_client_get_profile_array_from_variant:
 **/
static GPtrArray *
cd_client_get_profile_array_from_variant (GVariant *result,
					 GCancellable *cancellable,
					 GError **error)
{
	CdProfile *profile;
	gboolean ret;
	gchar *object_path_tmp;
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;
	guint i;
	guint len;
	GVariant *child = NULL;
	GVariantIter iter;

	/* add each profile */
	array_tmp = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	child = g_variant_get_child_value (result, 0);
	len = g_variant_iter_init (&iter, child);
	for (i=0; i < len; i++) {
		g_variant_get_child (child, i,
				     "o", &object_path_tmp);
		g_debug ("%s", object_path_tmp);

		/* create profile and add to the array */
		profile = cd_profile_new ();
		ret = cd_profile_set_object_path_sync (profile,
						      object_path_tmp,
						      cancellable,
						      &error_local);
		if (!ret) {
			g_set_error (error,
				     CD_CLIENT_ERROR,
				     CD_CLIENT_ERROR_FAILED,
				     "Failed to set profile object path: %s",
				     error_local->message);
			g_error_free (error_local);
			g_object_unref (profile);
			goto out;
		}
		g_ptr_array_add (array_tmp, profile);
		g_free (object_path_tmp);
	}

	/* success */
	array = g_ptr_array_ref (array_tmp);
out:
	if (child != NULL)
		g_variant_unref (child);
	if (array_tmp != NULL)
		g_ptr_array_unref (array_tmp);
	return array;
}

/**
 * cd_client_get_profiles_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Get an array of the profile objects.
 *
 * Return value: (transfer full): an array of #CdProfile objects,
 *		 free with g_ptr_array_unref()
 *
 * Since: 0.1.0
 **/
GPtrArray *
cd_client_get_profiles_sync (CdClient *client,
			    GCancellable *cancellable,
			    GError **error)
{
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GVariant *result;

	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->proxy != NULL, NULL);

	result = g_dbus_proxy_call_sync (client->priv->proxy,
					 "GetProfiles",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 cancellable,
					 &error_local);
	if (result == NULL) {
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to GetProfiles: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* convert to array of CdProfile's */
	array = cd_client_get_profile_array_from_variant (result,
							 cancellable,
							 error);
	if (array == NULL)
		goto out;
out:
	if (result != NULL)
		g_variant_unref (result);
	return array;
}

/**
 * cd_client_dbus_signal_cb:
 **/
static void
cd_client_dbus_signal_cb (GDBusProxy *proxy,
			  gchar      *sender_name,
			  gchar      *signal_name,
			  GVariant   *parameters,
			  CdClient   *client)
{
	gchar *object_path_tmp = NULL;

	if (g_strcmp0 (signal_name, "Changed") == 0) {
		g_warning ("changed");
	} else if (g_strcmp0 (signal_name, "DeviceAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		//EMIT
	} else if (g_strcmp0 (signal_name, "DeviceRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		//EMIT
	} else if (g_strcmp0 (signal_name, "ProfileAdded") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		//EMIT
	} else if (g_strcmp0 (signal_name, "ProfileRemoved") == 0) {
		g_variant_get (parameters, "(o)", &object_path_tmp);
		//EMIT
	} else {
		g_warning ("unhandled signal '%s'", signal_name);
	}
	g_free (object_path_tmp);
}

/**
 * cd_client_connect_sync:
 * @client: a #CdClient instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Connects to the colord daemon.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.1.0
 **/
gboolean
cd_client_connect_sync (CdClient *client, GCancellable *cancellable, GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	GVariant *daemon_version = NULL;

	g_return_val_if_fail (CD_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy == NULL, FALSE);

	/* connect to the daemon */
	client->priv->proxy =
		g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       COLORD_DBUS_SERVICE,
					       COLORD_DBUS_PATH,
					       COLORD_DBUS_INTERFACE,
					       cancellable,
					       &error_local);
	if (client->priv->proxy == NULL) {
		ret = FALSE;
		g_set_error (error,
			     CD_CLIENT_ERROR,
			     CD_CLIENT_ERROR_FAILED,
			     "Failed to connect to colord: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* get daemon version */
	daemon_version = g_dbus_proxy_get_cached_property (client->priv->proxy,
							   "Title");
	if (daemon_version != NULL)
		client->priv->daemon_version = g_variant_dup_string (daemon_version, NULL);

	/* get signals from DBus */
	g_signal_connect (client->priv->proxy,
			  "g-signal",
			  G_CALLBACK (cd_client_dbus_signal_cb),
			  client);

	/* success */
	g_debug ("Connected to colord daemon version %s",
		 client->priv->daemon_version);
out:
	if (daemon_version != NULL)
		g_variant_unref (daemon_version);
	return ret;
}

/**
 * cd_client_get_daemon_version:
 * @client: a #CdClient instance.
 *
 * Get colord daemon version.
 *
 * Return value: string containing the daemon version, e.g. 0.1.0
 *
 * Since: 0.1.0
 **/
const gchar *
cd_client_get_daemon_version (CdClient *client)
{
	g_return_val_if_fail (CD_IS_CLIENT (client), NULL);
	return client->priv->daemon_version;
}

/*
 * cd_client_get_property:
 */
static void
cd_client_get_property (GObject *object,
			 guint prop_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	CdClient *client;
	client = CD_CLIENT (object);

	switch (prop_id) {
	case PROP_DAEMON_VERSION:
		g_value_set_string (value, client->priv->daemon_version);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * cd_client_class_init:
 */
static void
cd_client_class_init (CdClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = cd_client_get_property;
	object_class->finalize = cd_client_finalize;

	/**
	 * CdClient:daemon-version:
	 *
	 * The daemon version.
	 *
	 * Since: 0.1.0
	 */
	g_object_class_install_property (object_class,
					 PROP_DAEMON_VERSION,
					 g_param_spec_string ("daemon-version",
							      "Daemon version",
							      NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * CdClient::device-added:
	 * @client: the #CdClient instance that emitted the signal
	 * @device: the #CdDevice that was added.
	 *
	 * The ::device-added signal is emitted when a device is added.
	 *
	 * Since: 0.1.0
	 **/
	signals [CD_CLIENT_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, device_added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);

	/**
	 * CdClient::device-removed:
	 * @client: the #CdClient instance that emitted the signal
	 * @device: the #CdDevice that was removed.
	 *
	 * The ::device-added signal is emitted when a device is removed.
	 *
	 * Since: 0.1.0
	 **/
	signals [CD_CLIENT_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, device_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, CD_TYPE_DEVICE);

	/**
	 * CdClient::changed:
	 * @client: the #CdDevice instance that emitted the signal
	 *
	 * The ::changed signal is emitted when properties may have changed.
	 *
	 * Since: 0.1.0
	 **/
	signals [CD_CLIENT_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdClientClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (CdClientPrivate));
}

/*
 * cd_client_init:
 */
static void
cd_client_init (CdClient *client)
{
	client->priv = CD_CLIENT_GET_PRIVATE (client);
}

/*
 * cd_client_finalize:
 */
static void
cd_client_finalize (GObject *object)
{
	CdClient *client = CD_CLIENT (object);

	g_return_if_fail (CD_IS_CLIENT (object));

	g_free (client->priv->daemon_version);
	if (client->priv->proxy != NULL)
		g_object_unref (client->priv->proxy);

	G_OBJECT_CLASS (cd_client_parent_class)->finalize (object);
}

/**
 * cd_client_new:
 *
 * Creates a new #CdClient object.
 *
 * Return value: a new CdClient object.
 *
 * Since: 0.1.0
 **/
CdClient *
cd_client_new (void)
{
	if (cd_client_object != NULL) {
		g_object_ref (cd_client_object);
	} else {
		cd_client_object = g_object_new (CD_TYPE_CLIENT, NULL);
		g_object_add_weak_pointer (cd_client_object, &cd_client_object);
	}
	return CD_CLIENT (cd_client_object);
}
