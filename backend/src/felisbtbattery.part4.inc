
	if (biev_ok)
		diagnostics.biev_handler_ok_count++;
	else
		diagnostics.biev_handler_fail_count++;
	if (xapl_ok)
		diagnostics.xapl_handler_ok_count++;
	else
		diagnostics.xapl_handler_fail_count++;
	if (iphone_ok)
		diagnostics.iphone_handler_ok_count++;
	else
		diagnostics.iphone_handler_fail_count++;

	if (!biev_ok)
		ofono_error(PLUGIN_NAME ": failed to install +BIEV handler");
	if (!xapl_ok)
		ofono_warn(PLUGIN_NAME ": +XAPL handler unavailable");
	if (!iphone_ok)
		ofono_warn(PLUGIN_NAME ": +IPHONEACCEV handler unavailable");

	ctx_unref(ctx);

	return -ENODEV;
}

static struct ofono_handsfree_card_driver tap_driver = {
	.name = HFP_AG_DRIVER,
	.probe = tap_probe
};

/*
 * Event-driven tap ordering.
 *
 * The stock hfp_ag_bluez5 plugin has default priority and therefore installs
 * its org.bluez service watch before this low-priority plugin.  When BlueZ
 * appears, stock hfp_ag_enable() registers /bluetooth/profile/hfp_ag and then
 * prepends the real hfp-ag-driver.  We must prepend our declining tap only
 * after that has happened.
 *
 * dbus_connection_get_object_path_data() gives us a local, in-process
 * readiness condition: the profile object path exists only after the stock
 * g_dbus_register_interface() call.  hfp_ag_enable() immediately registers
 * the stock card driver in the same synchronous callback; if that fails it
 * removes the profile interface again before returning.  Therefore, once a
 * later main-loop callback observes the path, the stock driver is ready.
 *
 * The 50 ms source below is only a retry cadence while waiting for that
 * condition; correctness no longer depends on an arbitrary delay.
 */
static gboolean stock_hfp_profile_ready(void)
{
	void *path_data = NULL;

	if (!bus_conn)
		return FALSE;

	if (!dbus_connection_get_object_path_data(bus_conn,
			HFP_AG_EXT_PROFILE_PATH, &path_data))
		return FALSE;

	return path_data != NULL;
}

static gboolean register_tap_when_ready(void)
{
	int err;

	diagnostics.stock_hfp_check_count++;

	if (!bluez_up || !stock_hfp_profile_ready())
		return FALSE;

	if (!stock_hfp_ready) {
		stock_hfp_ready = TRUE;
		diagnostics.stock_hfp_ready_count++;
		ofono_info(PLUGIN_NAME ": stock HFP AG is ready");
	}

	if (tap_driver_registered)
		return TRUE;

	err = ofono_handsfree_card_driver_register(&tap_driver);
	if (err < 0) {
		diagnostics.driver_register_fail_count++;
		ofono_error(PLUGIN_NAME ": tap driver registration failed: %d", err);
		return TRUE;
	}

	diagnostics.driver_register_count++;
	tap_driver_registered = TRUE;
	ofono_info(PLUGIN_NAME ": tap driver registered after stock HFP AG");
	return TRUE;
}

static gboolean stock_hfp_check_cb(gpointer user_data)
{
	(void)user_data;

	if (register_tap_when_ready()) {
		stock_hfp_check_id = 0;
		return G_SOURCE_REMOVE;
	}

	if (!bluez_up) {
		stock_hfp_check_id = 0;
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void start_stock_hfp_check(void)
{
	if (!bluez_up || tap_driver_registered || stock_hfp_check_id)
		return;

	if (register_tap_when_ready())
		return;

	stock_hfp_check_id = g_timeout_add(STOCK_HFP_CHECK_MS,
			stock_hfp_check_cb, NULL);
	if (!stock_hfp_check_id)
		ofono_error(PLUGIN_NAME ": unable to schedule stock HFP readiness check");
}

static void bluez_connect_cb(DBusConnection *connection, void *user_data)
{
	(void)connection;
	(void)user_data;

	bluez_up = TRUE;
	diagnostics.bluez_connect_count++;
	ofono_info(PLUGIN_NAME ": BlueZ available; waiting for stock HFP AG");
	start_stock_hfp_check();
}

static void bluez_disconnect_cb(DBusConnection *connection, void *user_data)
{
	(void)connection;
	(void)user_data;

	diagnostics.bluez_disconnect_count++;
	bluez_up = FALSE;
	stock_hfp_ready = FALSE;

	if (stock_hfp_check_id) {
		g_source_remove(stock_hfp_check_id);
		stock_hfp_check_id = 0;
	}

	if (tap_driver_registered) {
		ofono_handsfree_card_driver_unregister(&tap_driver);
		tap_driver_registered = FALSE;
		diagnostics.driver_unregister_count++;
	}

	provider_state = PROVIDER_NONE;
	g_free(provider_adapter);
	provider_adapter = NULL;

	ofono_info(PLUGIN_NAME ": BlueZ unavailable; tap driver disarmed");
}

static void unregister_provider(void)
{
	DBusMessage *msg;
	const char *root = PROVIDER_ROOT;

	if (!bus_conn || provider_state != PROVIDER_REGISTERED ||
			!provider_adapter)
		return;

	msg = dbus_message_new_method_call(BLUEZ_SERVICE, provider_adapter,
				BATTERY_PROVIDER_MANAGER_IFACE,
				"UnregisterBatteryProvider");
	if (!msg)
		return;

	dbus_message_append_args(msg,
			DBUS_TYPE_OBJECT_PATH, &root,
			DBUS_TYPE_INVALID);
	dbus_connection_send(bus_conn, msg, NULL);
	dbus_message_unref(msg);
	provider_state = PROVIDER_NONE;
}

static int felisbtbattery_init(void)
{
	bus_conn = ofono_dbus_get_connection();
	if (!bus_conn)
		return -EIO;

	if (!dbus_connection_register_fallback(bus_conn, PROVIDER_ROOT,
				&provider_vtable, NULL)) {
		ofono_error(PLUGIN_NAME ": unable to register provider object path");
		bus_conn = NULL;
		return -EIO;
	}

	bluez_watch_id = g_dbus_add_service_watch(bus_conn, BLUEZ_SERVICE,
			bluez_connect_cb, bluez_disconnect_cb, NULL, NULL);
	if (!bluez_watch_id) {
		dbus_connection_unregister_object_path(bus_conn, PROVIDER_ROOT);
		bus_conn = NULL;
		return -EIO;
	}

	ofono_info(PLUGIN_NAME ": plugin loaded; event-driven HFP tap ordering active");
	return 0;
}

static void felisbtbattery_exit(void)
{
	if (bluez_watch_id) {
		g_dbus_remove_watch(bus_conn, bluez_watch_id);
		bluez_watch_id = 0;
	}

	if (stock_hfp_check_id) {
		g_source_remove(stock_hfp_check_id);
		stock_hfp_check_id = 0;
	}

	if (tap_driver_registered) {
		ofono_handsfree_card_driver_unregister(&tap_driver);
		tap_driver_registered = FALSE;
	}

	while (contexts) {
		struct battery_ctx *ctx = contexts->data;
		ctx_ref(ctx);
		ofono_emulator_remove_handler(ctx->em, "+BIEV");
		ofono_emulator_remove_handler(ctx->em, "+XAPL");
		ofono_emulator_remove_handler(ctx->em, "+IPHONEACCEV");
		ctx_unref(ctx);
	}

	unregister_provider();
	if (bus_conn)
		dbus_connection_unregister_object_path(bus_conn, PROVIDER_ROOT);

	g_free(provider_adapter);
	provider_adapter = NULL;
	provider_state = PROVIDER_NONE;
	bluez_up = FALSE;
	stock_hfp_ready = FALSE;

	g_free(diagnostics.last_remote);
	g_free(diagnostics.last_device_path);
	g_free(diagnostics.last_source);
	memset(&diagnostics, 0, sizeof(diagnostics));
	bus_conn = NULL;
}

OFONO_PLUGIN_DEFINE(felisbtbattery,
		"Bluetooth accessory battery provider for FelisBTBattery",
		OFONO_VERSION, OFONO_PLUGIN_PRIORITY_LOW,
		felisbtbattery_init, felisbtbattery_exit)
