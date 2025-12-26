// static-status.js

(function () {
  // Semaphore-like flag to control if relays should be loaded or not
  let isUpdating = false;

  function formatTimeSinceBoot(microseconds) {
    let total_seconds = Math.floor(microseconds / 1000000);

    let days = Math.floor(total_seconds / (24 * 60 * 60));
    total_seconds %= (24 * 60 * 60);

    let hours = Math.floor(total_seconds / (60 * 60));
    total_seconds %= (60 * 60);

    let minutes = Math.floor(total_seconds / 60);
    let seconds = total_seconds % 60;

    return `${days} days ${hours} hours ${minutes} minutes ${seconds} seconds`;
  }

  function updateStatusData() {
    $.ajax({
      url: "/api/status",
      type: "GET",
      dataType: "json",
      success: function (response) {
        $("#val_free_heap").text(response.status.free_heap);
        $("#val_min_free_heap").text(response.status.min_free_heap);

        let time_since_boot = formatTimeSinceBoot(response.status.time_since_boot);
        $("#val_time_since_boot").text(time_since_boot);
      },
      error: function () {
        console.error("Failed to fetch device status data");
      }
    });
  }

  function loadRelays() {
    if (isUpdating) {
      return;
    }

    $.ajax({
      url: "/api/relays",
      type: "GET",
      dataType: "json",
      success: function (response) {
        $("#blockRelayState").empty();

        let actuatorsHtml = '<h3>Actuators</h3><table class="table table-striped">';
        actuatorsHtml += "<thead><tr><th>Relay Key</th><th>Channel</th><th>GPIO</th><th>State</th></tr></thead><tbody>";

        let sensorsHtml = '<h3>Contact Sensors</h3><table class="table table-striped">';
        sensorsHtml += "<thead><tr><th>Relay Key</th><th>Channel</th><th>GPIO</th><th>Contact State (to GND)</th></tr></thead><tbody>";

        response.data.forEach(function (relay) {
          if (relay.type === 0) {
            let checked = relay.state ? "checked" : "";
            let rowHtml = `<tr>
              <td>${relay.relay_key}</td>
              <td>${relay.channel}</td>
              <td>${relay.gpio_pin}</td>
              <td>
                <div class="form-check form-switch">
                  <input class="form-check-input" type="checkbox" role="switch"
                         id="switch_${relay.relay_key}" ${checked}
                         onchange="RelayBoardStatus.changeRelayState('${relay.relay_key}', ${relay.channel}, this.checked)">
                  <label class="form-check-label" for="switch_${relay.relay_key}">
                    ${relay.state ? "ON" : "OFF"}
                  </label>
                </div>
              </td>
            </tr>`;
            actuatorsHtml += rowHtml;
          } else if (relay.type === 1) {
            let stateText = relay.state ? "CLOSED" : "OPEN";
            let rowHtml = `<tr>
              <td>${relay.relay_key}</td>
              <td>${relay.channel}</td>
              <td>${relay.gpio_pin}</td>
              <td>${stateText}</td>
            </tr>`;
            sensorsHtml += rowHtml;
          }
        });

        actuatorsHtml += "</tbody></table>";
        sensorsHtml += "</tbody></table>";

        $("#blockRelayState").append(actuatorsHtml + sensorsHtml);
      },
      error: function () {
        console.error("Failed to fetch relay data from the API");
      }
    });
  }

  // Expose only what the HTML needs to call from inline handlers
  window.RelayBoardStatus = window.RelayBoardStatus || {};
  
  window.RelayBoardStatus.changeRelayState = function (relayKey, channel, isChecked) {
    isUpdating = true;

    const deviceId = window.RelayBoardStatus.deviceId || "";
    const deviceSerial = window.RelayBoardStatus.deviceSerial || "";

    let payload = {
      device_id: deviceId,
      device_serial: deviceSerial,
      data: {
        relay_key: relayKey,
        relay_channel: channel,
        relay_state: isChecked
      }
    };

    $.ajax({
      url: "/api/relay/update",
      type: "POST",
      contentType: "application/json",
      data: JSON.stringify(payload),
      success: function () {
        isUpdating = false;
        loadRelays();
      },
      error: function () {
        console.error(`Failed to update relay ${relayKey}`);
        isUpdating = false;
      }
    });
  };

  // Entry point
  $(document).ready(function () {
    const intervalMs = window.RelayBoardStatus.readIntervalMs || 5000;

    updateStatusData();
    loadRelays();

    setInterval(updateStatusData, intervalMs);
    setInterval(loadRelays, intervalMs);
  });
})();