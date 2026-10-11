	}

	if (ctx->remote && find_device_in_managed_objects(reply, ctx->remote,
				&ctx->device_path, &ctx->adapter_path)) {
		const char *suffix = strrchr(ctx->device_path, '/');

		diagnostics.resolve_success_count++;
		g_free(diagnostics.last_remote);
		diagnostics.last_remote = g_strdup(ctx->remote);
		g_free(diagnostics.last_device_path);
		diagnostics.last_device_path = g_strdup(ctx->device_path);
		ctx->battery_path = g_strdup_printf(PROVIDER_ROOT "/battery%s",
				suffix ? suffix : ctx->device_path);
		ofono_info(PLUGIN_NAME ": matched %s -> %s",
				ctx->remote, ctx->device_path);
		provider_register(ctx->adapter_path);
		if (provider_state == PROVIDER_REGISTERED)
			emit_interfaces_added(ctx);
	} else {
		ofono_warn(PLUGIN_NAME ": BlueZ device for %s not found",
				ctx->remote ? ctx->remote : "unknown");
	}

	dbus_message_unref(reply);
}

static void resolve_device(struct battery_ctx *ctx)
{
	DBusMessage *msg;
	DBusPendingCall *pending = NULL;
	const char *remote;

	if (!bus_conn || ctx->device_path || ctx->resolving)
		return;

	remote = ofono_handsfree_card_get_remote(ctx->card);
	if (remote && !ctx->remote)
		ctx->remote = g_strdup(remote);
	if (!ctx->remote)
		return;

	msg = dbus_message_new_method_call(BLUEZ_SERVICE, "/", BLUEZ_OM_IFACE,
				"GetManagedObjects");
	if (!msg)
		return;

	ctx->resolving = TRUE;
	diagnostics.resolve_attempt_count++;
	if (!dbus_connection_send_with_reply(bus_conn, msg, &pending, -1) ||
			!pending) {
		ctx->resolving = FALSE;
		dbus_message_unref(msg);
		return;
	}

	dbus_pending_call_set_notify(pending, resolve_device_reply,
				ctx_ref(ctx), ctx_unref);
	dbus_pending_call_unref(pending);
	dbus_message_unref(msg);
}

static void provider_register_reply(DBusPendingCall *pending, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(pending);
	GSList *l;

	(void)user_data;
	if (!reply) {
		provider_state = PROVIDER_NONE;
		return;
	}

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		ofono_error(PLUGIN_NAME ": RegisterBatteryProvider failed: %s",
				dbus_message_get_error_name(reply));
		provider_state = PROVIDER_NONE;
		dbus_message_unref(reply);
		return;
	}

	provider_state = PROVIDER_REGISTERED;
	diagnostics.provider_register_success_count++;
	ofono_info(PLUGIN_NAME ": BlueZ battery provider registered on %s",
			provider_adapter);

	/* These objects are discovered by BlueZ through GetManagedObjects. */
	for (l = contexts; l; l = l->next) {
		struct battery_ctx *ctx = l->data;
		if (ctx_visible_to_provider(ctx))
			ctx->exposed = TRUE;
	}

	dbus_message_unref(reply);
}

static void provider_register(const char *adapter_path)
{
	DBusMessage *msg;
	DBusPendingCall *pending = NULL;
	const char *root = PROVIDER_ROOT;

	if (!bus_conn || !adapter_path)
		return;

	if (provider_adapter && g_strcmp0(provider_adapter, adapter_path)) {
		ofono_warn(PLUGIN_NAME ": second Bluetooth adapter %s ignored; active %s",
				adapter_path, provider_adapter);
		return;
	}

	if (provider_state == PROVIDER_REGISTERED ||
			provider_state == PROVIDER_PENDING)
		return;

	g_free(provider_adapter);
	provider_adapter = g_strdup(adapter_path);

	msg = dbus_message_new_method_call(BLUEZ_SERVICE, adapter_path,
				BATTERY_PROVIDER_MANAGER_IFACE,
				"RegisterBatteryProvider");
	if (!msg)
		return;

	dbus_message_append_args(msg,
			DBUS_TYPE_OBJECT_PATH, &root,
			DBUS_TYPE_INVALID);

	provider_state = PROVIDER_PENDING;
	diagnostics.provider_register_attempt_count++;
	if (!dbus_connection_send_with_reply(bus_conn, msg, &pending, -1) ||
			!pending) {
		provider_state = PROVIDER_NONE;
		dbus_message_unref(msg);
		return;
	}

	dbus_pending_call_set_notify(pending, provider_register_reply, NULL, NULL);
	dbus_pending_call_unref(pending);
	dbus_message_unref(msg);
}

static void report_battery(struct battery_ctx *ctx, guint8 percentage,
				const char *source)
{
	gboolean changed = !ctx->battery_known || ctx->percentage != percentage ||
			g_strcmp0(ctx->source, source);

	ctx->percentage = percentage;
	ctx->battery_known = TRUE;
	g_free(ctx->source);
	ctx->source = g_strdup(source);

	diagnostics.battery_report_count++;
	diagnostics.last_battery_known = TRUE;
	diagnostics.last_percentage = percentage;
	g_free(diagnostics.last_source);
	diagnostics.last_source = g_strdup(source);
	if (ctx->remote) {
		g_free(diagnostics.last_remote);
		diagnostics.last_remote = g_strdup(ctx->remote);
	}

	if (!ctx->device_path) {
		resolve_device(ctx);
		return;
	}

	provider_register(ctx->adapter_path);

	if (provider_state != PROVIDER_REGISTERED)
		return;

	if (!ctx->exposed)
		emit_interfaces_added(ctx);
	else if (changed)
		emit_percentage_changed(ctx);
}

static void biev_cb(struct ofono_emulator *em,
			struct ofono_emulator_request *req, void *user_data)
{
	struct battery_ctx *ctx = user_data;
	int indicator, value;

	diagnostics.biev_count++;
	if (ofono_emulator_request_get_type(req) !=
			OFONO_EMULATOR_REQUEST_TYPE_SET)
		goto fail;

	if (!ofono_emulator_request_next_number(req, &indicator) ||
			!ofono_emulator_request_next_number(req, &value))
		goto fail;

	if (indicator == 2) {
		if (value < 0 || value > 100)
			goto fail;
		report_battery(ctx, (guint8)value, "HFP 1.7 HF indicator");
		send_final(em, TRUE);
		return;
	}

	/* Preserve oFono's existing Enhanced Safety response semantics enough
	 * not to reject indicator 1. The private ddr_active state is not part
	 * of the public plugin API. */
	if (indicator == 1 && value >= 0 && value <= 1) {
		send_final(em, TRUE);
		return;
	}

fail:
	send_final(em, FALSE);
}

static void xapl_cb(struct ofono_emulator *em,
			struct ofono_emulator_request *req, void *user_data)
{
	(void)user_data;
	diagnostics.xapl_count++;
	if (ofono_emulator_request_get_type(req) !=
			OFONO_EMULATOR_REQUEST_TYPE_SET) {
		send_final(em, FALSE);
		return;
	}

	ofono_emulator_send_intermediate(em, "+XAPL=iPhone,6");
	send_final(em, TRUE);
}

static void iphoneaccev_cb(struct ofono_emulator *em,
			struct ofono_emulator_request *req, void *user_data)
{
	struct battery_ctx *ctx = user_data;
	int count, i;
	gboolean got_battery = FALSE;
	int battery_value = -1;

	diagnostics.iphoneaccev_count++;
	if (ofono_emulator_request_get_type(req) !=
			OFONO_EMULATOR_REQUEST_TYPE_SET)
		goto fail;

	if (!ofono_emulator_request_next_number(req, &count) ||
			count < 0 || count > 20)
		goto fail;

	for (i = 0; i < count; i++) {
		int key, value;
		if (!ofono_emulator_request_next_number(req, &key) ||
				!ofono_emulator_request_next_number(req, &value))
			goto fail;
		if (key == 1 && value >= 0 && value <= 9) {
			got_battery = TRUE;
			battery_value = value;
		}
	}

	if (got_battery)
		report_battery(ctx, (guint8)((battery_value + 1) * 10),
				"Apple accessory indication");

	send_final(em, TRUE);
	return;

fail:
	send_final(em, FALSE);
}

static gboolean add_handler_with_ref(struct battery_ctx *ctx,
				const char *prefix,
				ofono_emulator_request_cb_t cb)
{
	ctx_ref(ctx);
	if (ofono_emulator_add_handler(ctx->em, prefix, cb, ctx, ctx_unref))
		return TRUE;
	ctx_unref(ctx);
	return FALSE;
}

static int tap_probe(struct ofono_handsfree_card *card,
			unsigned int vendor, void *data)
{
	struct ofono_emulator *em = data;
	struct battery_ctx *ctx;
	gboolean biev_ok, xapl_ok, iphone_ok;

	(void)vendor;

	diagnostics.probe_count++;
	ctx = g_new0(struct battery_ctx, 1);
	ctx->refs = 1;
	ctx->em = em;
	ctx->card = card;
	contexts = g_slist_prepend(contexts, ctx);

	/* Stock oFono only accepts BIEV indicator 1. Replace only BIEV; keep
	 * stock BIND untouched because it participates in SLC completion. */
	ofono_emulator_remove_handler(em, "+BIEV");
	biev_ok = add_handler_with_ref(ctx, "+BIEV", biev_cb);
	xapl_ok = add_handler_with_ref(ctx, "+XAPL", xapl_cb);
	iphone_ok = add_handler_with_ref(ctx, "+IPHONEACCEV", iphoneaccev_cb);
