// script-relays.js

// 1) Update relay configuration
function updateRelay(deviceSerial, relayKey, relayChannel, gpioPinFieldId, enabledFieldId, invertedFieldId) {
          // Read values from the input fields
          const gpioPinValue = parseInt(document.getElementById(gpioPinFieldId).value, 10);
          const enabledValue = document.getElementById(enabledFieldId).checked;
          const invertedValue = document.getElementById(invertedFieldId).checked;
      
          // Prepare the data to be sent in JSON format
          const relayData = {
              data: {
                  device_serial: deviceSerial,
                  relay_key: relayKey,           // Add relay_key to the JSON data
                  relay_channel: relayChannel,
                  relay_gpio_pin: gpioPinValue,  // Send GPIO pin as a number
                  relay_enabled: enabledValue,   // Send enabled as boolean
                  relay_inverted: invertedValue  // Send inverted as boolean
              }
          };
      
          // Send the JSON data to the /update-relay endpoint via POST
          $.ajax({
              url: '/api/relay/update',
              type: 'POST',
              contentType: 'application/json',
              data: JSON.stringify(relayData),
              success: function(response) {
                  // Display success message next to the "Update" button
                  $("#status_" + relayKey).text("Status: " + response.status.error);
      
                  // Check if the update was successful
                  if (response.status.code === 0) {
                      // Update the input fields with values from the response
                      const updatedData = response.data;
      
                      // Update GPIO pin input
                      document.getElementById(gpioPinFieldId).value = updatedData.gpio_pin;
      
                      // Update enabled checkbox
                      document.getElementById(enabledFieldId).checked = updatedData.enabled;
      
                      // Update inverted checkbox
                      document.getElementById(invertedFieldId).checked = updatedData.inverted;
                  }
              },
              error: function(xhr, status, error) {
                  // Handle any error and display the error message
                  $("#status_" + relayKey).text("Error: " + error);
              }
          });
      }