		return dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS,
				"Unsupported interface");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &props);
	append_battery_properties(&props, ctx);
	dbus_message_iter_close_container(&iter, &props);
	return reply;
}

static struct battery_ctx *find_ctx_by_battery_path(const char *path)
{
	GSList *l;

	for (l = contexts; l; l = l->next) {
		struct battery_ctx *ctx = l->data;
		if (ctx->battery_path && !g_strcmp0(ctx->battery_path, path))
			return ctx;
	}
	return NULL;
}

static const char *provider_state_name(void)
{
	switch (provider_state) {
	case PROVIDER_PENDING:
		return "pending";
	case PROVIDER_REGISTERED:
		return "registered";
	case PROVIDER_NONE:
	default:
		return "none";
	}
}

static DBusMessage *make_diagnostics_reply(DBusMessage *msg)
{
	DBusMessage *reply;
	DBusMessageIter iter, dict;
	guint32 context_count = g_slist_length(contexts);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);

	append_variant_string(&dict, "Version", PLUGIN_VERSION);
	append_variant_uint32(&dict, "ContextCount", context_count);
	append_variant_uint32(&dict, "ProbeCount", diagnostics.probe_count);
	append_variant_uint32(&dict, "BievCount", diagnostics.biev_count);
	append_variant_uint32(&dict, "XaplCount", diagnostics.xapl_count);
	append_variant_uint32(&dict, "IphoneAccevCount", diagnostics.iphoneaccev_count);
	append_variant_uint32(&dict, "BatteryReportCount", diagnostics.battery_report_count);
	append_variant_uint32(&dict, "ResolveAttemptCount", diagnostics.resolve_attempt_count);
	append_variant_uint32(&dict, "ResolveSuccessCount", diagnostics.resolve_success_count);
	append_variant_uint32(&dict, "ProviderRegisterAttemptCount", diagnostics.provider_register_attempt_count);
	append_variant_uint32(&dict, "ProviderRegisterSuccessCount", diagnostics.provider_register_success_count);
	append_variant_uint32(&dict, "BievHandlerOkCount", diagnostics.biev_handler_ok_count);
	append_variant_uint32(&dict, "BievHandlerFailCount", diagnostics.biev_handler_fail_count);
	append_variant_uint32(&dict, "XaplHandlerOkCount", diagnostics.xapl_handler_ok_count);
	append_variant_uint32(&dict, "XaplHandlerFailCount", diagnostics.xapl_handler_fail_count);
	append_variant_uint32(&dict, "IphoneHandlerOkCount", diagnostics.iphone_handler_ok_count);
	append_variant_uint32(&dict, "IphoneHandlerFailCount", diagnostics.iphone_handler_fail_count);
	append_variant_uint32(&dict, "DriverRegisterCount", diagnostics.driver_register_count);
	append_variant_uint32(&dict, "DriverRegisterFailCount", diagnostics.driver_register_fail_count);
	append_variant_uint32(&dict, "DriverDelayedRegisterCount", diagnostics.driver_delayed_register_count);
	append_variant_uint32(&dict, "DriverUnregisterCount", diagnostics.driver_unregister_count);
	append_variant_uint32(&dict, "BluezConnectCount", diagnostics.bluez_connect_count);
	append_variant_uint32(&dict, "BluezDisconnectCount", diagnostics.bluez_disconnect_count);
	append_variant_uint32(&dict, "StockHfpCheckCount", diagnostics.stock_hfp_check_count);
	append_variant_uint32(&dict, "StockHfpReadyCount", diagnostics.stock_hfp_ready_count);
	append_variant_bool(&dict, "BluezUp", bluez_up);
	append_variant_bool(&dict, "StockHfpReady", stock_hfp_ready);
	append_variant_bool(&dict, "StockHfpCheckPending", stock_hfp_check_id != 0);
	append_variant_bool(&dict, "TapDriverRegistered", tap_driver_registered);
	append_variant_string(&dict, "ProviderState", provider_state_name());
	append_variant_bool(&dict, "LastBatteryKnown", diagnostics.last_battery_known);

	if (provider_adapter)
		append_variant_string(&dict, "ProviderAdapter", provider_adapter);
	if (diagnostics.last_remote)
		append_variant_string(&dict, "LastRemote", diagnostics.last_remote);
	if (diagnostics.last_device_path)
		append_variant_string(&dict, "LastDevicePath", diagnostics.last_device_path);
	if (diagnostics.last_source)
		append_variant_string(&dict, "LastSource", diagnostics.last_source);
	if (diagnostics.last_battery_known)
		append_variant_byte(&dict, "LastPercentage", diagnostics.last_percentage);

	dbus_message_iter_close_container(&iter, &dict);
	return reply;
}

static DBusHandlerResult provider_message(DBusConnection *connection,
				DBusMessage *msg, void *user_data)
{
	const char *path = dbus_message_get_path(msg);
	DBusMessage *reply = NULL;
	struct battery_ctx *ctx;

	(void)user_data;

	if (!g_strcmp0(path, PROVIDER_ROOT) &&
			dbus_message_is_method_call(msg, BLUEZ_OM_IFACE,
					"GetManagedObjects")) {
		reply = make_managed_objects_reply(msg);
	} else if (!g_strcmp0(path, PROVIDER_ROOT) &&
			dbus_message_is_method_call(msg, DIAGNOSTICS_IFACE,
					"GetDiagnostics")) {
		reply = make_diagnostics_reply(msg);
	} else if ((ctx = find_ctx_by_battery_path(path)) &&
			dbus_message_is_method_call(msg, DBUS_PROPERTIES_IFACE,
					"GetAll")) {
		reply = make_getall_reply(msg, ctx);
	}

	if (!reply)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_connection_send(connection, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusObjectPathVTable provider_vtable = {
	.message_function = provider_message
};

static void emit_interfaces_added(struct battery_ctx *ctx)
{
	DBusMessage *sig;
	DBusMessageIter iter, interfaces;
	const char *path;

	if (!bus_conn || !ctx_visible_to_provider(ctx) || ctx->exposed)
		return;

	path = ctx->battery_path;
	sig = dbus_message_new_signal(PROVIDER_ROOT, BLUEZ_OM_IFACE,
				"InterfacesAdded");
	if (!sig)
		return;

	dbus_message_iter_init_append(sig, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
				"{sa{sv}}", &interfaces);
	append_battery_interface(&interfaces, ctx);
	dbus_message_iter_close_container(&iter, &interfaces);

	dbus_connection_send(bus_conn, sig, NULL);
	dbus_message_unref(sig);
	ctx->exposed = TRUE;
}

static void emit_interfaces_removed(struct battery_ctx *ctx)
{
	DBusMessage *sig;
	DBusMessageIter iter, array;
	const char *path;
	const char *iface = BATTERY_PROVIDER_IFACE;

	if (!bus_conn || !ctx->battery_path || !ctx->exposed)
		return;

	path = ctx->battery_path;
	sig = dbus_message_new_signal(PROVIDER_ROOT, BLUEZ_OM_IFACE,
				"InterfacesRemoved");
	if (!sig)
		return;

	dbus_message_iter_init_append(sig, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &array);
	dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &iface);
	dbus_message_iter_close_container(&iter, &array);

	dbus_connection_send(bus_conn, sig, NULL);
	dbus_message_unref(sig);
	ctx->exposed = FALSE;
}

static void emit_percentage_changed(struct battery_ctx *ctx)
{
	DBusMessage *sig;
	DBusMessageIter iter, changed, invalidated;
	const char *iface = BATTERY_PROVIDER_IFACE;

	if (!bus_conn || !ctx->exposed || !ctx->battery_path)
		return;

	sig = dbus_message_new_signal(ctx->battery_path,
				DBUS_PROPERTIES_IFACE, "PropertiesChanged");
	if (!sig)
		return;

	dbus_message_iter_init_append(sig, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}",
				&changed);
	append_variant_byte(&changed, "Percentage", ctx->percentage);
	if (ctx->source)
		append_variant_string(&changed, "Source", ctx->source);
	dbus_message_iter_close_container(&iter, &changed);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s",
				&invalidated);
	dbus_message_iter_close_container(&iter, &invalidated);

	dbus_connection_send(bus_conn, sig, NULL);
	dbus_message_unref(sig);
}

static char *device_suffix_from_remote(const char *remote)
{
	char *copy, *p, *result;

	if (!remote || !*remote)
		return NULL;

	copy = g_ascii_strup(remote, -1);
	for (p = copy; *p; p++) {
		if (*p == ':')
			*p = '_';
	}
	result = g_strdup_printf("/dev_%s", copy);
	g_free(copy);
	return result;
}

static gboolean find_device_in_managed_objects(DBusMessage *reply,
				const char *remote,
				char **device_path,
				char **adapter_path)
{
	DBusMessageIter iter, objects;
	char *suffix;
	gboolean found = FALSE;

	if (!dbus_message_iter_init(reply, &iter) ||
			dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		return FALSE;

	suffix = device_suffix_from_remote(remote);
	if (!suffix)
		return FALSE;

	dbus_message_iter_recurse(&iter, &objects);
	while (dbus_message_iter_get_arg_type(&objects) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry;
		const char *path = NULL;

		dbus_message_iter_recurse(&objects, &entry);
		if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_OBJECT_PATH)
			dbus_message_iter_get_basic(&entry, &path);

		if (path && g_str_has_prefix(path, "/org/bluez/") &&
				g_str_has_suffix(path, suffix)) {
			gsize path_len = strlen(path);
			gsize suffix_len = strlen(suffix);
			*device_path = g_strdup(path);
			*adapter_path = g_strndup(path, path_len - suffix_len);
			found = TRUE;
			break;
		}
		dbus_message_iter_next(&objects);
	}

	g_free(suffix);
	return found;
}

static void provider_register(const char *adapter_path);

static void resolve_device_reply(DBusPendingCall *pending, void *user_data)
{
	struct battery_ctx *ctx = user_data;
	DBusMessage *reply = dbus_pending_call_steal_reply(pending);

	ctx->resolving = FALSE;

	if (!reply)
		return;

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		ofono_warn(PLUGIN_NAME ": BlueZ GetManagedObjects failed: %s",
				dbus_message_get_error_name(reply));
		dbus_message_unref(reply);
		return;
	}

	if (!ctx->remote) {
		const char *remote = ofono_handsfree_card_get_remote(ctx->card);
		if (remote)
			ctx->remote = g_strdup(remote);
