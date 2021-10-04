import { UAParser } from 'ua-parser-js';

export function getServerUrlFromLocation() {
  if (location) {
    if (location.protocol === 'http:') {
      return 'http://' + location.hostname + ':9559';
    } else if (location.protocol === 'https:') {
      return 'https://' + location.hostname + ':5443';
    }
  }
  return null;
}

export function getServerUrlFromLocalhost() {
  if (location && location.protocol === 'https:') {
    return 'https://127.0.0.1:5443';
  }
  return 'http://127.0.0.1:9559';
}

function getQueryString() {
  const queries = window.location.search.substr(1).split('&');
  if (queries === '') {
    return {};
  }

  const query = {};
  for (let i = 0; i < queries.length; i++) {
    const value = queries[i].split('=', 2);
    if (value.length === 1) {
      query[value[0]] = '';
    } else {
      query[value[0]] = decodeURIComponent(value[1].replace(/\+/g, ' '));
    }
  }
  return query;
}

export function isDebugMode() {
  const queries = getQueryString();
  return !!(queries['debug'] === 'true');
}

export function optionEnabled(key) {
  const queries = getOptions();
  return !!(queries[key] === 'true');
}

export function getOptions() {
  const options = {};
  const validOptions = ['debug', 'info'];
  const queries = getQueryString();
  for (const query in queries) {
    if (validOptions.indexOf(query) !== -1) {
      options[query] = queries[query];
    }
  }
  return options;
}

export function getDeviceName() {
  const ua = new UAParser().getResult();
  let deviceName = 'Unknown';
  if (ua.device.vendor && ua.device.model) {
    deviceName = `[${ua.device.vendor}] ${ua.device.model}`;
  } else if (ua.os.name && ua.browser.name) {
    deviceName = `[${ua.os.name}] ${ua.browser.name}`;
  }
  return deviceName;
}

export function getClientId() {
  let deviceName = localStorage.getItem('clientId');
  if (deviceName === null) {
    const ua = new UAParser().getResult();
    let header = 'Unknown';

    if (typeof ua.device.vendor !== 'undefined') {
      header = ua.device.vendor;
    } else if (typeof ua.os.name !== 'undefined') {
      header = ua.os.name;
    }

    deviceName = header + getUniqueId();
    localStorage.setItem('clientId', deviceName);
  }

  return deviceName;
}

/**
 * Determine whether the given `promise` is a Promise.
 * @param {*} promise
 * @return {Boolean}
 */
export function isPromise(promise) {
  return !!promise && typeof promise.then === 'function';
}

export function getUniqueId() {
  // Math.random should be unique because of its seeding algorithm.
  // Convert it to base 36 (numbers + letters), and grab the first 9 characters
  // after the decimal.
  return '_' + Math.random().toString(36).substr(2, 9);
}

export function getOffloadId(funcName) {
  return funcName + '_' + funcName.toLowerCase().split('').reverse().join('');
}
