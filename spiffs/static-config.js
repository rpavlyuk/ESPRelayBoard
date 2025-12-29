// script-config.js

function showConfigMessage(msg, isError) {
  // You currently render: <h5>{VAL_MESSAGE}</h5>
  // If you keep it, add id="cfg_msg" to that element (recommended).
  // Fallback: try to find it anyway.
  const $msg = $("#cfg_msg");
  if ($msg.length) {
    $msg.text(msg || "");
    $msg.toggleClass("text-danger", !!isError);
    $msg.toggleClass("text-success", !isError && !!msg);
  } else {
    // no dedicated element; at least log
    if (isError) console.error(msg);
    else console.log(msg);
  }
}

// Config page logic (CA cert lazy-load)
// Expect HTML to set: window.RelayBoardConfig = { deviceId, deviceSerial }
$(function () {
  if (!window.RelayBoardConfig) {
    return; // not on config page (or config not injected)
  }

  const deviceId = window.RelayBoardConfig.deviceId;
  const deviceSerial = window.RelayBoardConfig.deviceSerial;

  const certLoaded = { mqtts: false, https: false };

  function getTextarea(caType) {
    return (caType === "mqtts") ? $("#ca_cert_mqtts_id") : $("#ca_cert_https_id");
  }

  function loadCaCert(caType, doneCb) {
    if (certLoaded[caType]) {
      if (doneCb) doneCb();
      return;
    }

    const $textarea = getTextarea(caType);

    $textarea
      .prop("disabled", true)
      .val("Loading " + caType.toUpperCase() + " CA certificate...");

    $.ajax({
      url: "/api/cert/get",
      method: "GET",
      dataType: "json",
      data: {
        device_id: deviceId,
        device_serial: deviceSerial,
        ca_type: caType
      },
      timeout: 10000
    })
      .done(function (resp) {
        if (resp && resp.status === 0 && typeof resp.cert === "string") {
          $textarea.val(resp.cert);
          certLoaded[caType] = true;
        } else {
          const msg = (resp && resp.msg) ? resp.msg : "unknown error";
          $textarea.val(
            "ERROR: Failed to load " + caType.toUpperCase() + " CA certificate\n\n" + msg
          );
        }
      })
      .fail(function (xhr, status) {
        $textarea.val(
          "ERROR: Failed to load " + caType.toUpperCase() + " CA certificate\n\n" +
          "HTTP error: " + status
        );
      })
      .always(function () {
        $textarea.prop("disabled", false);
        if (doneCb) doneCb();
      });
  }

  // Sequential load on page load: MQTTS -> HTTPS
  loadCaCert("mqtts", function () {
    loadCaCert("https");
  });
});

// Load and populate config settings form
(function () {

  function getField(data, key) {
    // data[key] is expected as { type, max_size, value, size }
    if (!data || !data[key]) return null;
    return data[key];
  }

  function fieldValue(field) {
    if (!field) return null;
    return field.value;
  }

  function setInputByName(name, field) {
    const $el = $(`[name="${name}"]`);
    if (!$el.length) return;

    const v = fieldValue(field);
    $el.val(v == null ? "" : String(v));

    // Optional: apply maxlength if present and input is text-like
    if (field && field.max_size && $el.attr("type") !== "number") {
      $el.attr("maxlength", String(field.max_size));
    }
  }

  function setInputById(id, field) {
    const $el = $(`#${id}`);
    if (!$el.length) return;

    const v = fieldValue(field);
    $el.val(v == null ? "" : String(v));

    // Optional: apply maxlength if present and input is text-like
    if (field && field.max_size && $el.attr("type") !== "number") {
      $el.attr("maxlength", String(field.max_size));
    }
  }

  function setNumberByName(name, field) {
    const $el = $(`[name="${name}"]`);
    if (!$el.length) return;

    const v = fieldValue(field);
    $el.val(v == null ? "" : String(v));
  }

  function setSelectById(id, field) {
    const $el = $(`#${id}`);
    if (!$el.length) return;

    const v = fieldValue(field);
    if (v == null) return;

    $el.val(String(v));
  }

  function populateForm(data) {
    // MQTT
    setSelectById("mqtt_connect", getField(data, "mqtt_connect"));
    setInputByName("mqtt_server",   getField(data, "mqtt_server"));
    setNumberByName("mqtt_port",    getField(data, "mqtt_port"));
    setInputByName("mqtt_protocol", getField(data, "mqtt_protocol"));
    setInputByName("mqtt_user",     getField(data, "mqtt_user"));
    setInputByName("mqtt_password", getField(data, "mqtt_password"));
    setInputByName("mqtt_prefix",   getField(data, "mqtt_prefix"));

    // Home Assistant
    setInputByName("ha_prefix",       getField(data, "ha_prefix"));
    setNumberByName("ha_upd_intervl", getField(data, "ha_upd_intervl"));

    // OTA
    setInputByName("ota_update_url",  getField(data, "ota_update_url"));

    // Relay params
    setNumberByName("relay_ch_count", getField(data, "relay_ch_count"));
    setNumberByName("relay_sn_count", getField(data, "relay_sn_count"));
    setNumberByName("relay_refr_int", getField(data, "relay_refr_int"));

    // Net logging
    setSelectById("net_log_type",    getField(data, "net_log_type"));
    setInputByName("net_log_host",   getField(data, "net_log_host"));
    setNumberByName("net_log_port",  getField(data, "net_log_port"));
    setSelectById("net_log_stdout",  getField(data, "net_log_stdout"));

    // memguard
    setSelectById("memgrd_mode",     getField(data, "memgrd_mode"));
    setNumberByName("memgrd_trshld", getField(data, "memgrd_trshld"));
  }

  function loadAllSettings() {
    if (!window.RelayBoardConfig) return;

    const deviceId = window.RelayBoardConfig.deviceId;
    const deviceSerial = window.RelayBoardConfig.deviceSerial;

    showConfigMessage("Loading settings...", false);

    $.ajax({
      url: "/api/setting/get/all",
      method: "GET",
      dataType: "json",
      data: {
        device_id: deviceId,
        device_serial: deviceSerial
      },
      timeout: 10000
    })
    .done(function (resp) {
      if (!resp || typeof resp !== "object") {
        showConfigMessage("ERROR: invalid API response", true);
        return;
      }

      // Your schema: { data: { key: {.., value: ..}}, total: N }
      if (!resp.data || typeof resp.data !== "object") {
        showConfigMessage("ERROR: response.data missing", true);
        return;
      }

      populateForm(resp.data);
      showConfigMessage("", false);
    })
    .fail(function (xhr, status) {
      showConfigMessage("ERROR: failed to load settings (" + status + ")", true);
    });
  }

  $(function () {
    loadAllSettings();
  });

})();

(function () {

  function showSaveMsg(text, isError) {
    const $m = $("#cfg_save_msg");
    if (!$m.length) return;

    $m.removeClass("alert alert-success alert-danger");
    if (!text) {
      $m.text("");
      return;
    }

    $m.addClass("alert " + (isError ? "alert-danger" : "alert-success"));
    $m.text(text);
  }

  function clearFieldErrors($form) {
    $form.find(".cfg-field-error").remove();
    $form.find(".is-invalid").removeClass("is-invalid");
  }

  function showFieldError($form, fieldName, message) {
    const $field = $form.find(`[name="${fieldName}"]`);
    if (!$field.length) return;

    $field.addClass("is-invalid");

    // Put error message right after field
    const $err = $(`<div class="cfg-field-error invalid-feedback" style="display:block;"></div>`);
    $err.text(message || "Invalid value");
    $field.after($err);
  }

  function collectFormData($form) {
    // Collect only your inputs/selects (excluding submit/reset)
    const data = {};
    $form.find("input, select, textarea").each(function () {
      const $el = $(this);
      const name = $el.attr("name");
      if (!name) return;

      const type = ($el.attr("type") || "").toLowerCase();
      if (type === "submit" || type === "reset" || type === "button") return;

      if (type === "checkbox") {
        // your backend probably expects 0/1 or bool; choose 0/1 for embedded friendliness
        data[name] = $el.is(":checked") ? 1 : 0;
      } else {
        if (type === "number") {
          const v = $el.val();
          // Convert to number or null if empty/invalid
          data[name] = (v === "") ? null : Number(v);
        } else {
          if (type === "select" || $el.is("select")) {
            // try to convert to number if possible
            const v = $el.val();
            const n = Number(v);
            data[name] = (v === "" || isNaN(n)) ? v : n;
          } else {
            // text, textarea, password, etc.
            data[name] = $el.val();
          }
        }
      }
    });

    return data;
  }

  function applyPerFieldErrors($form, resp) {
    // Supports resp.data[field].status/msg OR resp.fields[field].status/msg
    const map = (resp && resp.data && typeof resp.data === "object") ? resp.data :
                (resp && resp.fields && typeof resp.fields === "object") ? resp.fields :
                null;

    if (!map) return false;

    let anyFieldErrors = false;

    Object.keys(map).forEach(function (k) {
      const entry = map[k];
      if (!entry || typeof entry !== "object") return;

      const st = (typeof entry.status === "number") ? entry.status : null;
      if (st !== null && st !== 0) {
        anyFieldErrors = true;
        showFieldError($form, k, entry.msg || ("Error code: " + st));
      }
    });

    return anyFieldErrors;
  }

  function bindAjaxSave() {
    const $form = $('form[action="/submit"]');
    if (!$form.length) return;

    $form.on("submit", function (e) {
      e.preventDefault();

      clearFieldErrors($form);
      showSaveMsg("Saving settings...", false);

      const cfg = window.RelayBoardConfig || {};
      const deviceId = cfg.deviceId || "";
      const deviceSerial = cfg.deviceSerial || "";

      const formData = collectFormData($form);

      // If your API expects nested: { data: { device_id, device_serial, ...fields } }
      // this matches your other API styles.
      // Build payload EXACTLY as your API expects
      const payload = {
        device_id: deviceId,
        device_serial: deviceSerial,
        action: 0,          // 0 = update (per your API)
        data: formData      // ONLY settings here
      };

      $.ajax({
        url: "/api/setting/update",
        type: "POST",
        contentType: "application/json",
        dataType: "json",
        data: JSON.stringify(payload),
        timeout: 15000
      })
      .done(function (resp) {
        const anyFieldErrors = applyPerFieldErrors($form, resp);

        // Determine global status
        const st = (resp && typeof resp.status === "number") ? resp.status : (anyFieldErrors ? 1 : 0);
        const msg = (resp && resp.msg) ? resp.msg : (st === 0 ? "Saved successfully" : "Failed to save");

        if (st === 0 && !anyFieldErrors) {
          showSaveMsg(msg, false);
        } else {
          showSaveMsg("ERROR: " + msg, true);
        }
      })
      .fail(function (xhr, status) {
        showSaveMsg("ERROR: Save failed (" + status + ")", true);
      });
    });
  }

  $(function () {
    bindAjaxSave();
  });

})();