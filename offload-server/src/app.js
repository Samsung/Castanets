const path = require('path');
const fs = require('fs');
const express = require('express');
const os = require('os');
const QRCode = require('qrcode');

const TAG = 'app.js';

const app = express();

const options = {
  key: fs.readFileSync(path.resolve(__dirname, 'key.pem')),
  cert: fs.readFileSync(path.resolve(__dirname, 'cert.pem'))
};

const httpPort = process.env.HTTP_PORT || 9559;
const httpsPort = process.env.PORT || process.env.HTTPS_PORT || 5443;
const httpsServer = require('https').createServer(options, app);
const httpServer = require('http').createServer(app);
const io = require('socket.io')();
const isTizen = process.platform === 'tizen';

console.log(TAG, `platform : ${process.platform}`);

io.attach(httpServer);
io.attach(httpsServer);

const clients = new Set();
const workers = new Map();
const sockets = new Map();
let edgeForCastanets = null;
let forceQuitTimer = null;
let isMeerkatStarted = false;

app.set('host', '0.0.0.0');

if (isTizen) {
  app.use(express.static(path.join(__dirname, './public')));
} else {
  app.use(
    '/offload.html',
    express.static(path.join(__dirname, '../../sample/src/offload.html'))
  );
  // Host offload-worker
  app.use(
    '/offload-worker',
    express.static(path.join(__dirname, '../../offload-worker/src/'))
  );
  app.use(
    '/offload-worker/offload-worker.js',
    express.static(path.join(__dirname, '../../dist/offload-worker.js'))
  );
  const serveIndex = require('serve-index');
  app.use(
    '/',
    express.static(path.join(__dirname, '../../dist')),
    express.static(path.join(__dirname, 'public')),
    express.static(path.join(__dirname, '../../sample/src')),
    serveIndex(path.join(__dirname, '../../sample/src'))
  );
  app.use(
    '/sample',
    express.static(path.join(__dirname, '../../sample')),
    serveIndex(path.join(__dirname, '../../sample'))
  );
  app.use(
    '/test',
    express.static(path.join(__dirname, '../../test')),
    serveIndex(path.join(__dirname, '../../test'))
  );
}

io.of('/offload-js').on('connection', function (socket) {
  if (isTizen && !isMeerkatStarted) {
    try {
      console.log(TAG, `Try to start Meerkat client.`);
      tizen.application.launch(
        'org.tizen.meerkat.client',
        () => {
          console.log(TAG, `Meerkat client is started.`);
          isMeerkatStarted = true;
        },
        err => console.error(TAG, 'Failed to launch Meerkat client. ' + err)
      );
    } catch (err) {
      console.error(TAG, 'Failed to launch Meerkat client. ' + err);
    }
  }
  console.log(TAG, `connection from '${socket.id}.`);
  sockets.set(socket.id, socket);

  // client creates a session.
  socket.on('create', async function () {
    if (forceQuitTimer !== null) {
      clearTimeout(forceQuitTimer);
    }

    if (clients.has(socket.id)) {
      console.log(TAG, `already created by ${socket.id}.`);
      return;
    }
    clients.add(socket.id);

    let qr = null;
    const myAddress = getMyAddress();
    if (myAddress) {
      try {
        qr = await QRCode.toDataURL(
          'https://' + myAddress + ':5443/offload-worker.html'
        );
      } catch (err) {
        console.error(TAG, 'unabled to generate QR: ' + error);
      }
    }

    socket.emit('greeting', {
      qrCode: qr,
      workers: Array.from(workers)
    });

    console.log(
      TAG,
      `[client] session created by ${socket.id}. workers.size : ${workers.size}`
    );

    if (supportEdgeOrchestration) {
      socket.emit(
        'capabilities',
        Array.from(edgeForCastanets.getCapabilities())
      );
    }
  });

  socket.on('getcapabilities', function () {
    console.log(TAG, `getcapabilities`);
    if (supportEdgeOrchestration) {
      socket.emit(
        'capabilities',
        Array.from(edgeForCastanets.getCapabilities())
      );
    } else {
      socket.emit('capabilities', []);
    }
  });

  socket.on('requestService', function (workerId) {
    if (supportEdgeOrchestration) {
      edgeForCastanets.requestService(workerId);
    }
  });

  // new worker has been joined.
  socket.on('join', function (worker) {
    if (supportEdgeOrchestration) {
      let deviceIp = socket.request.connection.remoteAddress;
      if (deviceIp.indexOf('::ffff:') !== -1) {
        deviceIp = deviceIp.substr(7, deviceIp.length);
      }

      if (deviceIp) {
        edgeForCastanets.joinDevice(deviceIp);
      }
    }

    workers.set(worker.id, {
      socketId: socket.id,
      name: worker.name,
      features: worker.features,
      mediaDeviceInfos: worker.mediaDeviceInfos,
      compute_tasks: 0
    });
    console.log(
      TAG,
      `worker[${workers.size}] join: '${worker.id}' - '${socket.id}', '${worker.name}'`,
      worker.features
    );

    for (const client of clients) {
      socket.to(client).emit('worker', {
        event: 'join',
        workerId: worker.id,
        socketId: socket.id,
        name: worker.name,
        features: worker.features,
        mediaDeviceInfos: worker.mediaDeviceInfos
      });
    }
  });

  // route message between clients.
  socket.on('message', function (data) {
    console.log(TAG, `message ${JSON.stringify(data)}`);
    let socketId = null;
    if (workers.has(data.to)) {
      socketId = workers.get(data.to).socketId;
    } else if (clients.has(data.to)) {
      socketId = data.to;
    }

    if (socketId) {
      socket.to(socketId).emit('message', data);
    }
  });

  socket.on('disconnect', function (reason) {
    sockets.delete(socket.id);
    if (clients.has(socket.id)) {
      console.log(TAG, `[client] session terminated by client: ${socket.id}`);

      // broadcast to offload-worker
      socket.broadcast.emit('client', {
        event: 'bye',
        socketId: socket.id
      });
      clients.delete(socket.id);

      if (clients.size === 0) {
        forceQuitTimer = setTimeout(function () {
          console.log(
            TAG,
            `All clients are destroyed. Broadcast 'forceQuit' to workers`
          );
          socket.broadcast.emit('client', {
            event: 'forceQuit',
            socketId: socket.id
          });
        }, 5000);
      }
    } else {
      if (supportEdgeOrchestration) {
        let deviceIp = socket.request.connection.remoteAddress;
        if (deviceIp.indexOf('::ffff:') !== -1) {
          deviceIp = deviceIp.substr(7, deviceIp.length);
        }

        if (deviceIp) {
          edgeForCastanets.disconnectDevice(deviceIp);
        }
      }

      let workerId = null;
      workers.forEach(function (value, key, map) {
        if (value.socketId === socket.id) {
          workerId = key;
        }
      });

      if (workerId) {
        for (const client of clients) {
          socket.to(client).emit('worker', {
            event: 'bye',
            workerId: workerId,
            socketId: socket.id
          });
        }
        workers.delete(workerId);
        console.log(TAG, `worker[${workers.size}] bye: '${workerId}'`);
      }
    }
  });
});

httpsServer.listen(httpsPort, function () {
  console.log(TAG, `server is listening on https ${httpsPort} port.`);
});
httpServer.listen(httpPort, function () {
  console.log(TAG, `server is listening on http ${httpPort} port.`);
});

function getMyAddress() {
  const interfaces = os.networkInterfaces();
  const addresses = {};
  for (const intf in interfaces) {
    if (interfaces.hasOwnProperty(intf)) {
      for (const addr in interfaces[intf]) {
        if (interfaces[intf].hasOwnProperty(addr)) {
          const address = interfaces[intf][addr];
          if (address.family === 'IPv4' && !address.internal) {
            addresses[intf] = address.address;
          }
        }
      }
    }
  }
  if (Object.keys(addresses).length === 0) {
    return null;
  }

  // Try to connect with 'wl' prefix interface first.
  const wlanKeys = Object.keys(addresses).filter(intf => /^wl/.test(intf));
  return wlanKeys.length > 0
    ? addresses[wlanKeys[0]]
    : Object.entries(addresses)[0][1];
}

// Implementation for edge orchestration
const supportEdgeOrchestration =
  typeof webapis !== 'undefined' && webapis.hasOwnProperty('edge');
console.log(TAG, `supportEdgeOrchestration : ${supportEdgeOrchestration}`);

class Edge {
  constructor(service, execType, packageName) {
    this.devices_ = new Set();
    this.capabilities_ = new Map();
    this.service_ = service;
    this.execType_ = execType;
    this.packageName_ = packageName;
    console.log(TAG, `Edge : ${this.service_}, ${this.packageName_}`);
  }

  getDevices() {
    return this.devices_;
  }

  joinDevice(joined) {
    console.log(TAG, `${joined} is joined`);
    this.devices_.add(joined);
  }

  disconnectDevice(disconnected) {
    if (this.devices_.has(disconnected)) {
      console.log(TAG, `'${disconnected} is disconnected`);
      this.devices_.delete(disconnected);
    }
  }

  getCapabilities(reload = true) {
    if (!reload) {
      return this.capabilities_;
    }
    const start = Date.now();
    this.capabilities_.clear();

    const execType = this.execType_;
    const deviceList = webapis.edge.orchestrationGetDevicelist(
      this.service_,
      execType
    );

    if (deviceList === null || deviceList.ipaddrs.length === 0) {
      console.log(TAG, `deviceList is null`);
      return this.capabilities_;
    }

    console.log(
      TAG,
      `${this.service_} deviceList : ${JSON.stringify(deviceList)}`
    );

    for (const ipaddr of deviceList.ipaddrs) {
      const features = webapis.edge.orchestrationReadCapability(ipaddr);
      console.log(TAG, `ReadCapability : ${ipaddr}, ${features.capability}`);
      try {
        let jsonCapability = JSON.parse(features.capability);
        if (!jsonCapability.hasOwnProperty('offloadjs')) {
          continue;
        }
        jsonCapability = jsonCapability['offloadjs'];

        this.capabilities_.set(jsonCapability.id, {
          ipaddr: ipaddr,
          name: jsonCapability.name,
          features: jsonCapability.features,
          options: null
        });
      } catch (err) {
        console.error(TAG, 'Failed to read capability : ' + err);
      }
    }
    console.log(TAG, `getCapabilities() ${Date.now() - start}ms`);
    return this.capabilities_;
  }

  requestService(workerId) {
    if (!this.capabilities_.has(workerId)) {
      return this;
    }

    const ipaddr = this.capabilities_.get(workerId).ipaddr;

    const execType = this.execType_;

    const myAddress = getMyAddress();
    if (!myAddress) {
      return this;
    }

    const parameter = `${this.packageName_} --type=offloadworker --signaling-server=https://${myAddress}:${httpsPort}/offload-js`;

    console.log(
      TAG,
      `RequestService : ${this.service_}, ${execType}, ${parameter}, ${ipaddr}`
    );
    webapis.edge.orchestrationRequestServiceOnDevice(
      this.service_,
      false,
      execType,
      parameter,
      ipaddr
    );

    return this;
  }
}

edgeForCastanets = new Edge(
  'castanets',
  'android',
  'com.samsung.android.castanets'
);
