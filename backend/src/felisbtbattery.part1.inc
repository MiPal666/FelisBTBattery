/*
 * FelisBTBattery oFono plugin
 *
 * Adds Bluetooth headset battery reporting to Sailfish oFono without
 * replacing the system ofonod binary.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <dbus/dbus.h>

#include <ofono/dbus.h>
#include <ofono/emulator.h>
#include <ofono/handsfree-audio.h>
#include <ofono/log.h>
#include <ofono/plugin.h>
#include <ofono/types.h>

/* g_dbus_* is exported by ofonod (src/ofono.ver), but gdbus.h is not part
 * of the public oFono SDK headers.  Keep the dependency limited to the two
 * watch functions we need. */
extern guint g_dbus_add_service_watch(DBusConnection *connection,
				const char *name,
				void (*connect)(DBusConnection *, void *),
				void (*disconnect)(DBusConnection *, void *),
			void *user_data, void (*destroy)(void *));
extern gboolean g_dbus_remove_watch(DBusConnection *connection, guint tag);

#define PLUGIN_NAME "felisbtbattery"
#define HFP_AG_DRIVER "hfp-ag-driver"
#define HFP_AG_EXT_PROFILE_PATH "/bluetooth/profile/hfp_ag"
#define STOCK_HFP_CHECK_MS 50

#define BLUEZ_SERVICE "org.bluez"
#define BLUEZ_OM_IFACE "org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"
#define BATTERY_PROVIDER_MANAGER_IFACE "org.bluez.BatteryProviderManager1"
#define BATTERY_PROVIDER_IFACE "org.bluez.BatteryProvider1"

#define PROVIDER_ROOT "/org/ofono/felisbtbattery"
#define DIAGNOSTICS_IFACE "org.feliscatus.FelisBTBattery.Diagnostics1"
#define PLUGIN_VERSION "0.2.0"

struct battery_ctx {
	gint refs;
	struct ofono_emulator *em;
	struct ofono_handsfree_card *card;
	char *remote;
	char *device_path;
	char *adapter_path;
	char *battery_path;
	char *source;
	guint8 percentage;
	gboolean battery_known;
	gboolean resolving;
	gboolean exposed;
};

static DBusConnection *bus_conn;
static GSList *contexts;
static char *provider_adapter;

enum provider_state {
	PROVIDER_NONE,
	PROVIDER_PENDING,
	PROVIDER_REGISTERED
};

static enum provider_state provider_state;
static guint bluez_watch_id;
static guint stock_hfp_check_id;
static gboolean bluez_up;
static gboolean stock_hfp_ready;
static gboolean tap_driver_registered;

struct diagnostics_state {
	guint32 probe_count;
	guint32 biev_count;
	guint32 xapl_count;
	guint32 iphoneaccev_count;
	guint32 battery_report_count;
	guint32 resolve_attempt_count;
	guint32 resolve_success_count;
	guint32 provider_register_attempt_count;
	guint32 provider_register_success_count;
	guint32 biev_handler_ok_count;
	guint32 biev_handler_fail_count;
	guint32 xapl_handler_ok_count;
	guint32 xapl_handler_fail_count;
	guint32 iphone_handler_ok_count;
	guint32 iphone_handler_fail_count;
	guint32 driver_register_count;
	guint32 driver_register_fail_count;
	guint32 driver_delayed_register_count;
	guint32 driver_unregister_count;
	guint32 bluez_connect_count;
	guint32 bluez_disconnect_count;
	guint32 stock_hfp_check_count;
	guint32 stock_hfp_ready_count;
	gboolean last_battery_known;
	guint8 last_percentage;
	char *last_remote;
	char *last_device_path;
	char *last_source;
};

static struct diagnostics_state diagnostics;

static struct battery_ctx *ctx_ref(struct battery_ctx *ctx)
{
	g_atomic_int_inc(&ctx->refs);
	return ctx;
}

static void emit_interfaces_removed(struct battery_ctx *ctx);

static void ctx_unref(void *data)
{
	struct battery_ctx *ctx = data;

	if (!ctx || !g_atomic_int_dec_and_test(&ctx->refs))
		return;

	if (ctx->exposed)
		emit_interfaces_removed(ctx);

	contexts = g_slist_remove(contexts, ctx);
	g_free(ctx->remote);
	g_free(ctx->device_path);
	g_free(ctx->adapter_path);
	g_free(ctx->battery_path);
	g_free(ctx->source);
	g_free(ctx);
}

static void send_final(struct ofono_emulator *em, gboolean ok)
{
	struct ofono_error result;

	memset(&result, 0, sizeof(result));
	result.type = ok ? OFONO_ERROR_TYPE_NO_ERROR : OFONO_ERROR_TYPE_FAILURE;
	ofono_emulator_send_final(em, &result);
}

static void append_variant_byte(DBusMessageIter *dict, const char *key,
				guint8 value)
{
	DBusMessageIter entry, variant;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "y", &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_BYTE, &value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(dict, &entry);
}

static void append_variant_uint32(DBusMessageIter *dict, const char *key,
				guint32 value)
{
	DBusMessageIter entry, variant;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(dict, &entry);
}

static void append_variant_bool(DBusMessageIter *dict, const char *key,
				gboolean value)
{
	DBusMessageIter entry, variant;
	dbus_bool_t b = value ? TRUE : FALSE;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &b);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(dict, &entry);
}

static void append_variant_string(DBusMessageIter *dict, const char *key,
				const char *value)
{
	DBusMessageIter entry, variant;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(dict, &entry);
}

static void append_variant_object_path(DBusMessageIter *dict, const char *key,
				const char *value)
{
	DBusMessageIter entry, variant;

	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "o", &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_OBJECT_PATH, &value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(dict, &entry);
}

static void append_battery_properties(DBusMessageIter *dict,
				struct battery_ctx *ctx)
{
	append_variant_byte(dict, "Percentage", ctx->percentage);
	append_variant_object_path(dict, "Device", ctx->device_path);
	if (ctx->source)
		append_variant_string(dict, "Source", ctx->source);
}

static void append_battery_interface(DBusMessageIter *interfaces,
				struct battery_ctx *ctx)
{
	DBusMessageIter iface_entry, props;
	const char *iface = BATTERY_PROVIDER_IFACE;

	dbus_message_iter_open_container(interfaces, DBUS_TYPE_DICT_ENTRY, NULL,
				&iface_entry);
	dbus_message_iter_append_basic(&iface_entry, DBUS_TYPE_STRING, &iface);
	dbus_message_iter_open_container(&iface_entry, DBUS_TYPE_ARRAY, "{sv}",
				&props);
	append_battery_properties(&props, ctx);
	dbus_message_iter_close_container(&iface_entry, &props);
	dbus_message_iter_close_container(interfaces, &iface_entry);
}

static void append_managed_battery(DBusMessageIter *objects,
				struct battery_ctx *ctx)
{
	DBusMessageIter object_entry, interfaces;
	const char *path = ctx->battery_path;

	dbus_message_iter_open_container(objects, DBUS_TYPE_DICT_ENTRY, NULL,
				&object_entry);
	dbus_message_iter_append_basic(&object_entry, DBUS_TYPE_OBJECT_PATH, &path);
	dbus_message_iter_open_container(&object_entry, DBUS_TYPE_ARRAY,
				"{sa{sv}}", &interfaces);
	append_battery_interface(&interfaces, ctx);
	dbus_message_iter_close_container(&object_entry, &interfaces);
	dbus_message_iter_close_container(objects, &object_entry);
}

static gboolean ctx_visible_to_provider(struct battery_ctx *ctx)
{
	return ctx->battery_known && ctx->device_path && ctx->battery_path &&
		provider_adapter && ctx->adapter_path &&
		!g_strcmp0(provider_adapter, ctx->adapter_path);
}

static DBusMessage *make_managed_objects_reply(DBusMessage *msg)
{
	DBusMessage *reply;
	DBusMessageIter iter, objects;
	GSList *l;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
				"{oa{sa{sv}}}", &objects);

	for (l = contexts; l; l = l->next) {
		struct battery_ctx *ctx = l->data;
		if (ctx_visible_to_provider(ctx))
			append_managed_battery(&objects, ctx);
	}

	dbus_message_iter_close_container(&iter, &objects);
	return reply;
}

static DBusMessage *make_getall_reply(DBusMessage *msg,
				struct battery_ctx *ctx)
{
	DBusMessage *reply;
	DBusMessageIter iter, props;
	const char *iface = NULL;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_STRING, &iface,
				DBUS_TYPE_INVALID))
		return dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS,
				"Expected interface name");

	if (g_strcmp0(iface, BATTERY_PROVIDER_IFACE))
