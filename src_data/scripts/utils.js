const setupForm = (formSelector, onResultCallback = null, noCastItems = []) => {
  const form = document.querySelector(formSelector);
  if (!form) {
    return;
  }

  form.querySelectorAll('input').forEach(item => {
    item.addEventListener('change', (e) => {
      e.target.setAttribute('aria-invalid', !e.target.checkValidity());
    })
  });

  form.addEventListener('submit', async (event) => {
    event.preventDefault();

    const url = form.action;
    let button = form.querySelector('button[type="submit"]');
    let defaultText;

    if (button) {
      defaultText = button.textContent;
      button.textContent = i18n("button.wait");
      button.setAttribute('disabled', true);
      button.setAttribute('aria-busy', true);
    }

    const onSuccess = (result) => {
      if (button) {
        button.textContent = i18n('button.saved');
        button.classList.add('success');
        button.removeAttribute('aria-busy');

        setTimeout(() => {
          button.removeAttribute('disabled');
          button.classList.remove('success', 'failed');
          button.textContent = defaultText;
        }, 5000);
      }
    };

    const onFailed = () => {
      if (button) {
        button.textContent = i18n('button.error');
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
        method: "POST",
        cache: "no-cache",
        credentials: "include",
        headers: {
          "Content-Type": "application/json"
        },
        body: form2json(fd, noCastItems)
      });

      if (!response.ok) {
        throw new Error('Response not valid');
      }

      const result = response.status != 204 ? (await response.json()) : null;
      onSuccess(result);

      if (onResultCallback instanceof Function) {
        onResultCallback(result);
      }

    } catch (err) {
      console.log(err);
      onFailed();
    }
  });
}

const setupNetworkScanForm = (formSelector, tableSelector) => {
  const form = document.querySelector(formSelector);
  if (!form) {
    console.error("form not found");
    return;
  }

  const url = form.action;
  let button = form.querySelector('button[type="submit"]');
  let defaultText;

  const onSubmitFn = async (event) => {
    if (event) {
      event.preventDefault();
    }

    if (button) {
      defaultText = button.innerHTML;
      button.innerHTML = i18n('button.wait');
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
        row.onclick = () => {
          const input = document.querySelector('input#sta-ssid');
          const ssid = this.getAttribute('data-ssid');
          if (!input || !ssid) {
            return;
          }

          input.value = ssid;
          input.focus();
        };

        row.insertCell().textContent = `#${i + 1}`;
        row.insertCell().innerHTML = result[i].hidden ? `<i>${result[i].bssid}</i>` : result[i].ssid;

        // info cell
        let infoCell = row.insertCell();

        // signal quality
        let signalQualityIcon = document.createElement("i");
        if (result[i].signalQuality > 80) {
          signalQualityIcon.classList.add('icons-wifi-strength-4');
        } else if (result[i].signalQuality > 60) {
          signalQualityIcon.classList.add('icons-wifi-strength-3');
        } else if (result[i].signalQuality > 40) {
          signalQualityIcon.classList.add('icons-wifi-strength-2');
        } else if (result[i].signalQuality > 20) {
          signalQualityIcon.classList.add('icons-wifi-strength-1');
        } else {
          signalQualityIcon.classList.add('icons-wifi-strength-0');
        }
        
        let signalQualityContainer = document.createElement("span");
        signalQualityContainer.setAttribute('data-tooltip', `${result[i].signalQuality}%`);
        signalQualityContainer.appendChild(signalQualityIcon);
        infoCell.appendChild(signalQualityContainer);

        // auth
        const authList = {
          0: "Open",
          1: "WEP",
          2: "WPA",
          3: "WPA2",
          4: "WPA/WPA2",
          5: "WPA/WPA2 Enterprise",
          6: "WPA3",
          7: "WPA2/WPA3",
          8: "WAPI",
          9: "OWE",
          10: "WPA3 Enterprise"
        };
        let authIcon = document.createElement("i");

        if (result[i].auth == 0) {
          authIcon.classList.add('icons-unlocked');
        } else {
          authIcon.classList.add('icons-locked');
        }

        let authContainer = document.createElement("span");
        authContainer.setAttribute('data-tooltip', (result[i].auth in authList) ? authList[result[i].auth] : "unknown");
        authContainer.appendChild(authIcon);
        infoCell.appendChild(authContainer);
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
        let response = await fetch(url, {
          cache: "no-cache",
          credentials: "include"
        });

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

const setupRestoreBackupForm = (formSelector) => {
  const form = document.querySelector(formSelector);
  if (!form) {
    return;
  }

  const url = form.action;
  let button = form.querySelector('button[type="submit"]');
  let defaultText;

  form.addEventListener('submit', async (event) => {
    event.preventDefault();

    if (button) {
      defaultText = button.textContent;
      button.textContent = i18n('button.wait');
      button.setAttribute('disabled', true);
      button.setAttribute('aria-busy', true);
    }

    const onSuccess = () => {
      if (button) {
        button.textContent = i18n('button.restored');
        button.classList.add('success');
        button.removeAttribute('aria-busy');

        setTimeout(() => {
          button.removeAttribute('disabled');
          button.classList.remove('success', 'failed');
          button.textContent = defaultText;
        }, 5000);
      }
    };

    const onFailed = () => {
      if (button) {
        button.textContent = i18n('button.error');
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
    reader.onload = async (event) => {
      try {
        const data = JSON.parse(event.target.result);
        console.log("Backup: ", data);

        if (data.settings != undefined) {
          let response = await fetch(url, {
            method: "POST",
            cache: "no-cache",
            credentials: "include",
            headers: {
              "Content-Type": "application/json"
            },
            body: JSON.stringify({"settings": data.settings})
          });

          if (!response.ok) {
            onFailed();
            return;
          }
        }

        if (data.sensors != undefined) {
          for (const sensorId in data.sensors) {
            const payload = {
              "sensors": {}
            };
            payload["sensors"][sensorId] = data.sensors[sensorId];

            const response = await fetch(url, {
              method: "POST",
              cache: "no-cache",
              credentials: "include",
              headers: {
                "Content-Type": "application/json"
              },
              body: JSON.stringify(payload)
            });

            if (!response.ok) {
              onFailed();
              return;
            }
          }
        }

        if (data.network != undefined) {
          let response = await fetch(url, {
            method: "POST",
            cache: "no-cache",
            credentials: "include",
            headers: {
              "Content-Type": "application/json"
            },
            body: JSON.stringify({"network": data.network})
          });

          if (!response.ok) {
            onFailed();
            return;
          }
        }

        onSuccess();

      } catch (err) {
        onFailed();
      }
    };
    reader.onerror = () => {
      console.log(reader.error);
    };
  });
}

const setupUpgradeForm = (formSelector) => {
  const form = document.querySelector(formSelector);
  if (!form) {
    return;
  }

  const url = form.action;
  let button = form.querySelector('button[type="submit"]');
  let defaultText;

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
          resItem.textContent += `: ${result.firmware.error}`;
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
          resItem.textContent += `: ${result.filesystem.error}`;
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
      button.textContent = i18n('button.error');
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

    hide('.upgrade-firmware-result');
    hide('.upgrade-filesystem-result');

    if (button) {
      defaultText = button.textContent;
      button.textContent = i18n('button.uploading');
      button.setAttribute('disabled', true);
      button.setAttribute('aria-busy', true);
    }

    try {
      let fd = new FormData(form);
      let response = await fetch(url, {
        method: "POST",
        cache: "no-cache",
        credentials: "include",
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


const setBusy = (busySelector, contentSelector, value, parent = undefined) => {
  if (!value) {
    hide(busySelector, parent);
    show(contentSelector, parent);

  } else {
    show(busySelector, parent);
    hide(contentSelector, parent);
  }
}

const setAriaState = (selector, value, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let item = parent.querySelector(selector);
  if (!item) {
    return;
  }

  item.setAttribute('aria-invalid', !value);
}

const setStatus = (selector, state, color = undefined, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let item = parent.querySelector(selector);
  if (!item) {
    return;
  }

  item.classList.forEach(cName => {
    if (cName.indexOf("icons-") === 0) {
      item.classList.remove(cName);
    }
  });

  item.classList.add(`icons-${state}`);
  
  if (color !== undefined) {
    item.classList.add(`icons-color-${color}`);
  }
}

const setState = (selector, state, parent = undefined) => {
  return setStatus(
    selector,
    state ? "success" : "error",
    state ? "green" : "gray",
    parent
  );
}

const setValue = (selector, value, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let items = parent.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    item.innerHTML = value;
  }
}

const appendValue = (selector, value, nl = null, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let items = parent.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    if (item.innerHTML.trim().length > 0) {
      if (nl !== null) {
        item.innerHTML += nl;
      }

      item.innerHTML += value;

    } else {
      item.innerHTML = value;
    }
  }
}

const setCheckboxValue = (selector, value, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let item = parent.querySelector(selector);
  if (!item) {
    return;
  }

  item.checked = value;
}

const setRadioValue = (selector, value, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let items = parent.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    item.checked = item.value == value;
  }
}

const setInputValue = (selector, value, attrs = {}, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let items = parent.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    item.value = value;

    if (attrs instanceof Object) {
      for (let attrKey of Object.keys(attrs)) {
        item.setAttribute(attrKey, attrs[attrKey]);
      }
    }
  }
}

const setSelectValue = (selector, value, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }
  
  let item = parent.querySelector(selector);
  if (!item) {
    return;
  }

  for (let option of item.options) {
    option.selected = option.value == value;
  }
}

const show = (selector, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let items = parent.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    if (item.classList.contains('hidden')) {
      item.classList.remove('hidden');
    }
  }
}

const hide = (selector, parent = undefined) => {
  if (parent === undefined) {
    parent = document;
  }

  let items = parent.querySelectorAll(selector);
  if (!items.length) {
    return;
  }

  for (let item of items) {
    if (!item.classList.contains('hidden')) {
      item.classList.add('hidden');
    }
  }
}

function unit2str(unitSystem, units = {}, defaultValue = '?') {
  return (unitSystem in units) 
    ? units[unitSystem] 
    : defaultValue;
}

const temperatureUnit = (unitSystem) => {
  return unit2str(unitSystem, {
    0: "°C",
    1: "°F"
  });
}

const pressureUnit = (unitSystem) => {
  return unit2str(unitSystem, {
    0: "bar",
    1: "psi"
  });
}

const volumeUnit = (unitSystem) => {
  return unit2str(unitSystem, {
    0: "L",
    1: "gal"
  });
}

const purposeUnit = (purpose, unitSystem) => {
  const tUnit = temperatureUnit(unitSystem);

  return unit2str(purpose, {
    0:    tUnit,
    1:    tUnit,
    2:    tUnit,
    3:    tUnit,
    4:    tUnit,
    5:    tUnit,
    6:    `${volumeUnit(unitSystem)}/${i18n('time.min')}`,
    7:    tUnit,
    8:    "%",
    248:  "%",
    249:  i18n('kw'),
    250:  "RPM",
    251:  "ppm",
    252:  pressureUnit(unitSystem),
    253:  "%",
    254:  tUnit
  }, null);
}

const memberIdToVendor = (memberId) => {
  // https://github.com/Jeroen88/EasyOpenTherm/blob/main/src/EasyOpenTherm.h
  // https://github.com/Evgen2/SmartTherm/blob/v0.7/src/Web.cpp
  const vendorList = {
    1:    "Baxi",
    2:    "AWB/Brink/Viessmann",
    4:    "ATAG/Baxi/Brötje/ELCO/GEMINOX",
    5:    "Itho Daalderop",
    6:    "IDEAL",
    8:    "Buderus/Bosch/Hoval",
    9:    "Ferroli",
    11:   "Remeha/De Dietrich",
    16:   "Unical",
    24:   "Vaillant/Bulex",
    27:   "Baxi",
    29:   "Itho Daalderop",
    33:   "Viessmann",
    41:   "Italtherm/Radiant",
    56:   "Baxi",
    131:  "Nefit",
    148:  "Navien",
    173:  "Intergas",
    247:  "Baxi Ampera",
    248:  "Zota Lux-X"
  };
  
  return (memberId in vendorList) 
    ? vendorList[memberId] 
    : "unknown vendor";
}

function form2json(data, noCastItems = []) {
  let method = function (object, pair) {
    let keys = pair[0].replace(/\]/g, '').split('[');
    let key = keys[0];
    let value = pair[1];

    if (!noCastItems.includes(keys.join('.'))) {
      if (value === 'true' || value === 'false') {
        value = value === 'true';

      } else if (typeof (value) === 'string' && value.trim() !== '' && !isNaN(value)) {
        value = parseFloat(value);
      }
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

function dec2hex(i) {
  let hex = parseInt(i).toString(16);
  if (hex.length % 2 != 0) {
    hex = `0${hex}`;
  }
  
  return hex.toUpperCase();
}