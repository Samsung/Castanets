import { WorkerManager } from '~/offload/core/worker-manager';
import { Worker } from '~/offload/core/worker';

describe('worker-manager.js', function () {
  let workerManager;

  const testGreeting = {
    qrCode: 'testQRCode',
    workers: [
      [
        'testWorkerId',
        {
          socketId: 'testWorkerSocketId',
          name: 'testWorker',
          features: ['COMPUTE', 'CAMERA'],
          mediaDeviceInfos: [
            {
              deviceId: 'default',
              kind: 'videoinput',
              label: 'default',
              groupId: 'testGroupId'
            }
          ],
          compute_tasks: 0
        }
      ]
    ]
  };

  before(function () {
    workerManager = WorkerManager.getInstance();
  });

  describe('#getInstance', function () {
    it('get a same instance', function () {
      const instance = WorkerManager.getInstance();
      expect(instance).to.be.an('object');
      expect(instance).to.be(workerManager);
    });
  });

  describe('constructor', function () {
    it('check the initial values of WorkerManager members', function () {
      expect(workerManager.workerInfos_).to.be.a(Map);
      expect(workerManager.activeWorkers_).to.be.a(Map);
      expect(workerManager.socket_).to.be.an('object');
    });
  });

  describe('#connect', function () {
    it('check if callbacks are properly registered', function () {
      workerManager.connect('http://127.0.0.1:9559/offload-js');

      expect(workerManager.socket_).not.to.equal(null);
      expect(workerManager.socket_._callbacks).to.have.property('$greeting');
      expect(workerManager.socket_._callbacks).to.have.property('$worker');
      expect(workerManager.socket_._callbacks).to.have.property('$message');
      expect(workerManager.socket_._callbacks).to.have.property('$connect');
      expect(workerManager.socket_._callbacks).to.have.property('$disconnect');
    });

    it('close is called if the socket is already connected', function () {
      const close = sinon.spy(workerManager.socket_, 'close');
      workerManager.connect('http://127.0.0.1:9559/offload-js');
      close.restore();
      expect(close.called).to.be.ok();
    });
  });

  describe('#initFromGreeting_', function () {
    it('called initFromGreeting_ with testWorkerInfo', function () {
      workerManager.initFromGreeting_(testGreeting);

      expect(workerManager.workerInfos_).to.be.a(Map);
      expect(workerManager.qrCode_).to.eql(testGreeting.qrCode);
      expect(workerManager.workerInfos_.get(testGreeting.workers[0])).to.eql(
        testGreeting.workers[1]
      );
    });
  });

  describe('#handleWorkerEvent_', function () {
    it('check whether new worker info is properly joined', function () {
      // Check the size of workerInfos_ before joining the new worker
      expect(workerManager.workerInfos_.size).to.equal(1);

      workerManager.handleWorkerEvent_({
        event: 'join',
        workerId: 'testWorkerId2',
        socketId: 'testWorkerSocketId2',
        name: 'testWorker2',
        features: ['CAMERA'],
        mediaDeviceInfos: [
          {
            deviceId: 'default',
            kind: 'videoinput',
            label: 'default',
            groupId: 'testGroupId'
          }
        ]
      });

      // Check the size of workerInfos_ after joining the new worker
      expect(workerManager.workerInfos_.size).to.equal(2);
    });

    it('check the same worker info is not added when joining with same workerId', function () {
      expect(workerManager.workerInfos_.size).to.equal(2);

      workerManager.handleWorkerEvent_({
        event: 'join',
        workerId: 'testWorkerId2',
        socketId: 'testWorkerSocketId2',
        name: 'testWorker2',
        features: ['CAMERA'],
        mediaDeviceInfos: [
          {
            deviceId: 'default',
            kind: 'videoinput',
            label: 'default',
            groupId: 'testGroupId'
          }
        ]
      });

      expect(workerManager.workerInfos_.size).to.equal(2);
    });

    it('check whether existing worker info is properly byed', function () {
      expect(workerManager.workerInfos_.size).to.equal(2);

      workerManager.handleWorkerEvent_({
        event: 'bye',
        workerId: 'testWorkerId2'
      });

      expect(workerManager.workerInfos_.size).to.equal(1);
    });
  });

  describe('#getOrCreateWorker', function () {
    it('Check whether worker is properly created', function () {
      expect(workerManager.activeWorkers_.size).to.equal(0);
      const worker = workerManager.getOrCreateWorker('testWorkerId');
      expect(worker).to.be.a(Worker);
      expect(workerManager.activeWorkers_.size).to.equal(1);
    });

    it('Check whether the existing worker is obtained without creating it', function () {
      const worker = workerManager.getOrCreateWorker('testWorkerId');
      expect(worker).to.be.a(Worker);
      expect(workerManager.activeWorkers_.size).to.equal(1);
    });
  });

  describe('#handleMessage_', function () {
    it('Check if the activeWorkers`s handleMessage is properly called', function () {
      const worker = workerManager.getOrCreateWorker('testWorkerId');
      const handleMessage = sinon.spy(worker, 'handleMessage');

      workerManager.handleMessage_({
        from: 'testWorkerId',
        message: {
          type: 'offer'
        }
      });

      handleMessage.restore();
      expect(handleMessage.called).to.be.ok();
    });
  });

  describe('#socketConnected_', function () {
    it('Check whether the socket is connected correctly', function () {
      const emit = sinon.spy(workerManager.socket_, 'emit');
      workerManager.socketConnected_();
      emit.restore();
      expect(emit.calledWith('create')).to.be.ok();
    });
  });

  describe('#getWorkerInfos', function () {
    it('Check whether the worker information is obtained correctly', function () {
      const workerInfos = workerManager.getWorkerInfos();
      expect(workerInfos).to.eql(workerManager.workerInfos_);
    });
  });

  describe('#sendMessage', function () {
    before(function () {
      workerManager.connect('http://127.0.0.1:9559/offload-js');
    });

    it('Check whether the message is correctly emited', function () {
      const emit = sinon.spy(workerManager.socket_, 'emit');
      workerManager.sendMessage('testWorkerId', {});
      emit.restore();
      expect(
        emit.calledWith('message', {
          to: 'testWorkerId',
          from: workerManager.socket_.id,
          message: {}
        })
      ).to.be.ok();
    });
  });

  describe('#getQrCode', function () {
    it('Check whether the QR code for worker is obtained correctly', function () {
      workerManager.getQrCode(qrCode =>
        expect(qrCode).to.eql(workerManager.qrCode_)
      );
    });
  });
});
