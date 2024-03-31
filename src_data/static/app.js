function setupForm(formSelector) {
  const form = document.querySelector(formSelector);
  if (!form) {
    return;
  }

  form.querySelectorAll('input').forEach(item => {
    item.addEventListener('change', (e) => {
      e.target.setAttribute('aria-invalid', !e.target.checkValidity());
    })
  });

  const url = form.action;
  let button = form.querySelector('button[type="submit"]');
  let defaultText;

  if (button) {
    defaultText = button.textContent;
  }

  form.addEventListener('submit', async (event) => {
    event.preventDefault();

    if (button) {
      button.textContent = 'Please wait...';
      button.setAttribute('disabled', true);
      button.setAttribute('aria-busy', true);
    }

    const onSuccess = (response) => {
      if (button) {
        button.textContent = 'Saved';
        button.classList.add('success');
        button.removeAttribute('aria-busy');

        setTimeout(() => {
          button.removeAttribute('disabled');
          button.classList.remove('success', 'failed');
          button.textContent = defaultText;
        }, 5000);
      }
    };

    const onFailed = (response) => {
      if (button) {
        button.textContent = 'Error';
        button.classList.add('failed');
        button.removeAttribute('aria-busy');

        setTimeout(() => {
          button.removeAttribute('disabled');
          button.classList.remove('success', 'failed');
          button.textContent = defaultText;
        }, 5000);
      }
    };

    try {
      let fd = new FormData(form);
      let checkboxes = form.querySelectorAll('input[type="checkbox"]');
      for (let checkbox of checkboxes) {
        fd.append(checkbox.getAttribute('name'), checkbox.checked);
      }

      let response = await fetch(url, {
        method: 'POST',
        cache: 'no-cache',
        headers: {
          'Content-Type': 'application/json'
        },
        body: form2json(fd)
      });

      if (response.ok) {
        onSuccess(response);

      } else {
        onFailed(response);
      }

    } catch (err) {
      onFailed(false);
    }
  });
}

function setupNetworkScanForm(formSelector, tableSelector) {
  const form = document.querySelector(formSelector);
  if (!form) {
    console.error("form not found");
    return;
  }

  const url = form.action;
  let button = form.querySelector('button[type="submit"]');
  let defaultText;

  if (button) {
    defaultText = button.innerHTML;
  }

  const onSubmitFn = async (event) => {
    if (event) {
      event.preventDefault();
    }

    if (button) {
      button.innerHTML = 'Please wait...';
      button.setAttribute('disabled', true);
      button.setAttribute('aria-busy', true);
    }

    let table = document.querySelector(tableSelector);
    if (!table) {
      console.error("table not found");
      return;
    }

    const onSuccess = async (response) => {
      let result = await response.json();
      console.log('networks: ', result);

      let tbody = table.querySelector('tbody');
      if (!tbody) {
        tbody = table.createTBody();
      }

      while (tbody.rows.length > 0) {
        tbody.rows[0].remove();
      }

      for (let i = 0; i < result.length; i++) {
        let row = tbody.insertRow(-1);
        row.classList.add("network");
        row.setAttribute('data-ssid', result[i].hidden ? '' : result[i].ssid);
        row.onclick = function () {
          const input = document.querySelector('input.sta-ssid');
          const ssid = this.getAttribute('data-ssid');
          if (!input || !ssid) {
            return;
          }

          input.value = ssid;
          input.focus();
        };

        row.insertCell().textContent = "#" + (i + 1);
        row.insertCell().innerHTML = result[i].hidden ? '<i>Hidden</i>' : result[i].ssid;

        const signalCell = row.insertCell();
        const signalElement = document.createElement("kbd");
        signalElement.textContent = result[i].signalQuality + "%";
        if (result[i].signalQuality > 60) {
          signalElement.classList.add('greatSignal');
        } else if (result[i].signalQuality > 40) {
          signalElement.classList.add('normalSignal');
        } else {
          signalElement.classList.add('badSignal');
        }
        signalCell.appendChild(signalElement);
      }

      if (button) {
        button.innerHTML = defaultText;
        button.removeAttribute('disabled');
        button.removeAttribute('aria-busy');
      }
    };

    const onFailed = async (response) => {
      table.classList.remove('hidden');

      if (button) {
        button.innerHTML = defaultText;
        button.removeAttribute('disabled');
        button.removeAttribute('aria-busy');
      }
    };

    let attempts = 6;
    let attemptFn = async () => {
      attempts--;

      try {
        let response = await fetch(url, { cache: 'no-cache' });

        if (response.status == 200) {
          await onSuccess(response);

        } else if (attempts <= 0) {
          await onFailed(response);

        } else {
          setTimeout(attemptFn, 5000);
        }

      } catch (err) {
        if (attempts <= 0) {
          onFailed(err);

        } else {
          setTimeout(attemptFn, 10000);
        }
      }
    };
    attemptFn();
  };

  form.addEventListener('submit', onSubmitFn);
  onSubmitFn();
}

function setupRestoreBackupForm(formSelector) {
  const form = document.querySelector(formSelector);
  if (!form) {
    return;
  }

  const url = form.action;
  let button = form.querySelector('button[type="submit"]');
  let defaultText;

  if (button) {
    defaultText = button.textContent;
  }

  form.addEventListener('submit', async (event) => {
    event.preventDefault();

    if (button) {
      button.textContent = 'Please wait...';
      button.setAttribute('disabled', true);
      button.setAttribute('aria-busy', true);
    }

    const onSuccess = (response) => {
      if (button) {
        button.textContent = 'Restored';
        button.classList.add('success');
        button.removeAttribute('aria-busy');

        setTimeout(() => {
          button.removeAttribute('disabled');
          button.classList.remove('success', 'failed');
          button.textContent = defaultText;
        }, 5000);
      }
    };

    const onFailed = (response) => {
      if (button) {
        button.textContent = 'Error';
        button.classList.add('failed');
        button.removeAttribute('aria-busy');

        setTimeout(() => {
          button.removeAttribute('disabled');
          button.classList.remove('success', 'failed');
          button.textContent = defaultText;
        }, 5000);
      }
    };

    const files = form.querySelector('#restore-file').files;
    if (files.length <= 0) {
      onFailed(false);
      return;
    }

    let reader = new FileReader();
    reader.readAsText(files[0]);
    reader.onload = async function () {
      try {
        let response = await fetch(url, {
          method: 'POST',
          cache: 'no-cache',
          headers: {
            'Content-Type': 'application/json'
          },
          body: reader.result
        });

        if (response.ok) {
          onSuccess(response);

        } else {
          onFailed(response);
        }

      } catch (err) {
        onFailed(false);
      }
    };
    reader.onerror = function () {
      console.log(reader.error);
    };
  });
}

function setupUpgradeForm(formSelector) {
  const form = document.querySelector(formSelector);
  if (!form) {
    return;
  }

  const url = form.action;
  let button = form.querySelector('button[type="submit"]');
  let defaultText;

  if (button) {
    defaultText = button.textContent;
  }

  const statusToText = (status) => {
    switch (status) {
      case 0:
        return "None";
      case 1:
        return "No file";
      case 2:
        return "Success";
      case 3:
        return "Prohibited";
      case 4:
        return "Aborted";
      case 5:
        return "Error on start";
      case 6:
        return "Error on write";
      case 7:
        return "Error on finish";
      default:
        return "Unknown";
    }
  };

  const onResult = async (response) => {
    if (!response) {
      return;
    }

    const result = await response.json();

    let resItem = form.querySelector('.upgrade-firmware-result');
    if (resItem && result.firmware.status > 1) {
      resItem.textContent = statusToText(result.firmware.status);
      resItem.classList.remove('hidden');

      if (result.firmware.status == 2) {
        resItem.classList.remove('failed');
        resItem.classList.add('success');
      } else {
        resItem.classList.remove('success');
        resItem.classList.add('failed');

        if (result.firmware.error != "") {
          resItem.textContent += ": " + result.firmware.error;
        }
      }
    }

    resItem = form.querySelector('.upgrade-filesystem-result');
    if (resItem && result.filesystem.status > 1) {
      resItem.textContent = statusToText(result.filesystem.status);
      resItem.classList.remove('hidden');

      if (result.filesystem.status == 2) {
        resItem.classList.remove('failed');
        resItem.classList.add('success');
      } else {
        resItem.classList.remove('success');
        resItem.classList.add('failed');

        if (result.filesystem.error != "") {
          resItem.textContent += ": " + result.filesystem.error;
        }
      }
    }
  };

  const onSuccess = (response) => {
    onResult(response);

    if (button) {
      button.textContent = defaultText;
      button.removeAttribute('aria-busy');

      setTimeout(() => {
        button.removeAttribute('disabled');
        button.classList.remove('success', 'failed');
        button.textContent = defaultText;
      }, 5000);
    }
  };

  const onFailed = (response) => {
    if (button) {
      button.textContent = 'Error';
      button.classList.add('failed');
      button.removeAttribute('aria-busy');

      setTimeout(() => {
        button.removeAttribute('disabled');
        button.classList.remove('success', 'failed');
        button.textContent = defaultText;
      }, 5000);
    }
  };

  form.addEventListener('submit', async (event) => {
    event.preventDefault();

    if (button) {
      button.textContent = 'Uploading...';
      button.setAttribute('disabled', true);
      button.setAttribute('aria-busy', true);
    }

    try {
      let fd = new FormData(form);
      let response = await fetch(url, {
        method: 'POST',
        cache: 'no-cache',
        body: fd
      });

      if (response.status >= 200 && response.status < 500) {
        onSuccess(response);

      } else {
        onFailed(response);
      }

    } catch (err) {
      onFailed(false);
    }
  });
}

async function loadNetworkStatus() {
  let response = await fetch('/api/network/status', { cache: 'no-cache' });
  let result = await response.json();

  setValue('.network-hostname', result.hostname);
  setValue('.network-mac', result.mac);
  setState('.network-connected', result.isConnected);
  setValue('.network-ssid', result.ssid);
  setValue('.network-signal', result.signalQuality);
  setValue('.network-ip', result.ip);
  setValue('.network-subnet', result.subnet);
  setValue('.network-gateway', result.gateway);
  setValue('.network-dns', result.dns);

  setBusy('.main-busy', '.main-table', false);
}

async function loadNetworkSettings() {
  let response = await fetch('/api/network/settings', { cache: 'no-cache' });
  let result = await response.json();

  setInputValue('.network-hostname', result.hostname);
  setCheckboxValue('.network-use-dhcp', result.useDhcp);
  setInputValue('.network-static-ip', result.staticConfig.ip);
  setInputValue('.network-static-gateway', result.staticConfig.gateway);
  setInputValue('.network-static-subnet', result.staticConfig.subnet);
  setInputValue('.network-static-dns', result.staticConfig.dns);
  setBusy('#network-settings-busy', '#network-settings', false);

  setInputValue('.sta-ssid', result.sta.ssid);
  setInputValue('.sta-password', result.sta.password);
  setInputValue('.sta-channel', result.sta.channel);
  setBusy('#sta-settings-busy', '#sta-settings', false);

  setInputValue('.ap-ssid', result.ap.ssid);
  setInputValue('.ap-password', result.ap.password);
  setInputValue('.ap-channel', result.ap.channel);
  setBusy('#ap-settings-busy', '#ap-settings', false);
}

async function loadSettings() {
  let response = await fetch('/api/settings', { cache: 'no-cache' });
  let result = await response.json();

  setCheckboxValue('.system-debug', result.system.debug);
  setCheckboxValue('.system-use-serial', result.system.useSerial);
  setCheckboxValue('.system-use-telnet', result.system.useTelnet);
  setRadioValue('.system-unit-system', result.system.unitSystem);
  setBusy('#system-settings-busy', '#system-settings', false);

  setCheckboxValue('.portal-use-auth', result.portal.useAuth);
  setInputValue('.portal-login', result.portal.login);
  setInputValue('.portal-password', result.portal.password);
  setBusy('#portal-settings-busy', '#portal-settings', false);

  setRadioValue('.opentherm-unit-system', result.opentherm.unitSystem);
  setInputValue('.opentherm-in-gpio', result.opentherm.inGpio < 255 ? result.opentherm.inGpio : '');
  setInputValue('.opentherm-out-gpio', result.opentherm.outGpio < 255 ? result.opentherm.outGpio : '');
  setInputValue('.opentherm-member-id-code', result.opentherm.memberIdCode);
  setCheckboxValue('.opentherm-dhw-present', result.opentherm.dhwPresent);
  setCheckboxValue('.opentherm-sw-mode', result.opentherm.summerWinterMode);
  setCheckboxValue('.opentherm-heating-ch2-enabled', result.opentherm.heatingCh2Enabled);
  setCheckboxValue('.opentherm-heating-ch1-to-ch2', result.opentherm.heatingCh1ToCh2);
  setCheckboxValue('.opentherm-dhw-to-ch2', result.opentherm.dhwToCh2);
  setCheckboxValue('.opentherm-dhw-blocking', result.opentherm.dhwBlocking);
  setCheckboxValue('.opentherm-sync-modulation-with-heating', result.opentherm.modulationSyncWithHeating);
  setBusy('#opentherm-settings-busy', '#opentherm-settings', false);

  setInputValue('.mqtt-server', result.mqtt.server);
  setInputValue('.mqtt-port', result.mqtt.port);
  setInputValue('.mqtt-user', result.mqtt.user);
  setInputValue('.mqtt-password', result.mqtt.password);
  setInputValue('.mqtt-prefix', result.mqtt.prefix);
  setInputValue('.mqtt-interval', result.mqtt.interval);
  setBusy('#mqtt-settings-busy', '#mqtt-settings', false);

  setRadioValue('.outdoor-sensor-type', result.sensors.outdoor.type);
  setInputValue('.outdoor-sensor-gpio', result.sensors.outdoor.gpio  < 255 ? result.sensors.outdoor.gpio : '');
  setInputValue('.outdoor-sensor-offset', result.sensors.outdoor.offset);
  setBusy('#outdoor-sensor-settings-busy', '#outdoor-sensor-settings', false);

  setRadioValue('.indoor-sensor-type', result.sensors.indoor.type);
  setInputValue('.indoor-sensor-gpio', result.sensors.indoor.gpio < 255 ? result.sensors.indoor.gpio : '');
  setInputValue('.indoor-sensor-offset', result.sensors.indoor.offset);
  setInputValue('.indoor-sensor-ble-addresss', result.sensors.indoor.bleAddresss);
  setBusy('#indoor-sensor-settings-busy', '#indoor-sensor-settings', false);

  setCheckboxValue('.extpump-use', result.externalPump.use);
  setInputValue('.extpump-gpio', result.externalPump.gpio < 255 ? result.externalPump.gpio : '');
  setInputValue('.extpump-pc-time', result.externalPump.postCirculationTime);
  setInputValue('.extpump-as-interval', result.externalPump.antiStuckInterval);
  setInputValue('.extpump-as-time', result.externalPump.antiStuckTime);
  setBusy('#extpump-settings-busy', '#extpump-settings', false);
}

async function loadVars() {
  let response = await fetch('/api/vars');
  let result = await response.json();

  let unitSystemStr;
  switch (result.system.unitSystem) {
    case 0:
      unitSystemStr = 'C';
      break;

    case 1:
      unitSystemStr = 'F';
      break;

    default:
      unitSystemStr = '?';
      break;
  }

  setState('.ot-connected', result.states.otStatus);
  setState('.ot-emergency', result.states.emergency);
  setState('.ot-heating', result.states.heating);
  setState('.ot-dhw', result.states.dhw);
  setState('.ot-flame', result.states.flame);
  setState('.ot-fault', result.states.fault);
  setState('.ot-diagnostic', result.states.diagnostic);
  setState('.ot-external-pump', result.states.externalPump);

  setValue('.ot-modulation', result.sensors.modulation);
  setValue('.ot-pressure', result.sensors.pressure);
  setValue('.ot-dhw-flow-rate', result.sensors.dhwFlowRate);
  setValue('.ot-fault-code', result.sensors.faultCode ? ("E" + result.sensors.faultCode) : "-");

  setValue('.indoor-temp', result.temperatures.indoor);
  setValue('.outdoor-temp', result.temperatures.outdoor);
  setValue('.heating-temp', result.temperatures.heating);
  setValue('.heating-setpoint-temp', result.parameters.heatingSetpoint);
  setValue('.heating-return-temp', result.temperatures.heatingReturn);
  setValue('.dhw-temp', result.temperatures.dhw);
  setValue('.exhaust-temp', result.temperatures.exhaust);

  setBusy('.ot-busy', '.ot-table', false);

  setValue('.unit-system', unitSystemStr);
  setValue('.version', result.system.version);
  setValue('.build-date', result.system.buildDate);
  setValue('.uptime', result.system.uptime);
  setValue('.uptime-days', Math.floor(result.system.uptime / 86400));
  setValue('.uptime-hours', Math.floor(result.system.uptime % 86400 / 3600));
  setValue('.uptime-min', Math.floor(result.system.uptime % 3600 / 60));
  setValue('.uptime-sec', Math.floor(result.system.uptime % 60));
  setValue('.total-heap', result.system.totalHeap);
  setValue('.free-heap', result.system.freeHeap);
  setValue('.min-free-heap', result.system.minFreeHeap);
  setValue('.max-free-block-heap', result.system.maxFreeBlockHeap);
  setValue('.min-max-free-block-heap', result.system.minMaxFreeBlockHeap);
  setValue('.reset-reason', result.system.resetReason);
  setState('.mqtt-connected', result.system.mqttConnected);

  setBusy('.system-busy', '.system-table', false);
}

function setBusy(busySelector, contentSelector, value) {
  let busy = document.querySelector(busySelector);
  let content = document.querySelector(contentSelector);
  if (!busy || !content) {
    return;
  }

  if (!value) {
    busy.classList.add('hidden');
    content.classList.remove('hidden');

  } else {
    busy.classList.remove('hidden');
    content.classList.add('hidden');
  }
}

function setState(selector, value) {
  let item = document.querySelector(selector);
  if (!item) {
    return;
  }

  item.setAttribute('aria-invalid', !value);
}

function setValue(selector, value) {
  let items = document.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    item.innerHTML = value;
  }
}

function setCheckboxValue(selector, value) {
  let item = document.querySelector(selector);
  if (!item) {
    return;
  }

  item.checked = value;
}

function setRadioValue(selector, value) {
  let items = document.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    item.checked = item.value == value;
  }
}

function setInputValue(selector, value) {
  let item = document.querySelector(selector);
  if (!item) {
    return;
  }

  item.value = value;
}


function form2json(data) {
  let method = function (object, pair) {
    let keys = pair[0].replace(/\]/g, '').split('[');
    let key = keys[0];
    let value = pair[1];
    if (value === 'true' || value === 'false') {
      value = value === 'true';
    } else if (typeof (value) === 'string' && value.trim() !== '' && !isNaN(value)) {
      value = parseFloat(value);
    }

    if (keys.length > 1) {
      let i, x, segment;
      let last = value;
      let type = isNaN(keys[1]) ? {} : [];
      value = segment = object[key] || type;

      for (i = 1; i < keys.length; i++) {
        x = keys[i];
        if (i == keys.length - 1) {
          if (Array.isArray(segment)) {
            segment.push(last);
          } else {
            segment[x] = last;
          }
        } else if (segment[x] == undefined) {
          segment[x] = isNaN(keys[i + 1]) ? {} : [];
        }
        segment = segment[x];
      }
    }

    object[key] = value;
    return object;
  }

  let object = Array.from(data).reduce(method, {});
  return JSON.stringify(object);
}