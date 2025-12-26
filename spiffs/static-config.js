// script-config.js

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
