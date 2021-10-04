import debug from 'debug';

const APP_NAME = 'offload';
const USE_CONSOLE = false;
const LOG_LEVELS = [
  'debug',
  'error',
  'info',
  'log',
  'warn',
  'time',
  'timeEnd',
  'table'
];

export default class Logger {
  constructor(prefix) {
    if (!prefix) {
      throw new Error('Need a prefix when creating Logger class');
    }

    if (USE_CONSOLE) {
      // Logger does not provide the caller information at inspector
      LOG_LEVELS.forEach(level => {
        this[level] = console[level];
      });
    } else {
      LOG_LEVELS.forEach(level => {
        // Because time log must match the same namespace, no prefix is added.
        if (level.indexOf('time') > -1) {
          this[level] = debug(`${APP_NAME}:TIME:`);
        } else {
          this[level] = debug(`${APP_NAME}:${level.toUpperCase()}:${prefix}`);
        }
        this[level].log = function (...params) {
          this.apply(this, params);
        }.bind(console[level]);
      });
      // TODO: it should be implemented by app side..
      this.setLogElement('offloadLogText');
    }
  }

  timeFormat_(date, useDate) {
    return (
      (useDate
        ? [
            ('0' + (date.getMonth() + 1)).slice(-2),
            ('0' + date.getDate()).slice(-2)
          ].join('-') + ' '
        : '') +
      [
        ('0' + (date.getHours() + 1)).slice(-2),
        ('0' + date.getMinutes()).slice(-2),
        ('0' + date.getSeconds()).slice(-2),
        ('00' + date.getMilliseconds()).slice(-3)
      ].join(':')
    );
  }

  makeLogMessage_(message, level) {
    const color = {
      debug: 'black',
      error: 'red',
      info: 'blue',
      log: 'black',
      warn: 'green',
      time: 'black',
      timeEnd: 'black',
      table: 'black'
    };
    const now = new Date();
    message =
      typeof message === 'object'
        ? JSON.stringify(message)
        : message.replace(/(\w+:\w+:[\w-_.]+|\%c)/g, '').trim();
    return (
      `<span style="color:${color[level]}">` +
      `${this.timeFormat_(now)} ${message}` +
      `</span><br>`
    );
  }

  setLogElement(targetId) {
    const log = document.getElementById(targetId);
    if (!log) {
      return;
    }
    const self = this;
    Object.keys(this).forEach(level => {
      this[level] = function (...params) {
        log.innerHTML += self.makeLogMessage_(params[0], level);
        this.apply(this, params);
      }.bind(this[level]);
    });
  }

  static toString(object, maxDepth, maxLength, depth) {
    depth = depth || 0;
    maxDepth = maxDepth || 3;
    maxLength = maxLength || 120;
    let str;
    if (depth++ >= maxDepth) {
      return object;
    }
    if (Array.isArray(object)) {
      str = '[\n';
      for (const key of object) {
        str += `${' '.repeat(depth)}${Logger.toString(
          key,
          maxDepth,
          maxLength,
          depth
        )},\n`;
      }
      str += `${' '.repeat(depth - 1)}]`;
    } else if (typeof object === 'object') {
      str = '{\n';
      // eslint-disable-next-line guard-for-in
      for (const key in object) {
        str += `${' '.repeat(depth)}${key}:${Logger.toString(
          object[key],
          maxDepth,
          maxLength,
          depth
        )},\n`;
      }
      str += `${' '.repeat(depth - 1)}}`;
    } else if (typeof object === 'string') {
      str = `'${object}'`;
    } else if (typeof object === 'function') {
      str = `<<function>>`;
    } else {
      str = `${object}`;
    }
    if (str.length < maxLength) {
      str = str.replace(/\n */g, ' ');
    }
    return str;
  }
}
