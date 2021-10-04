import { Resource } from '~/offload/core/resource';

describe('core/resource.js', () => {
  let resource;

  before(() => {
    resource = new Resource();
  });

  describe('constructor', () => {
    it('check the initial values of resource members', () => {
      expect(resource.worker_).to.equal(null);
      expect(resource.alwaysConnect_).to.equal(null);
    });
  });

  describe('#startJob', () => {
    it('startJob when alwaysConnect_ is true', () => {
      resource.alwaysConnect_ = true;

      const spy = sinon.spy(resource, 'startJobImpl');
      const fakeJob = {};
      resource.startJob(fakeJob);
      spy.restore();
      expect(spy.calledOnceWith(resource.alwaysConnect_, fakeJob)).to.be.ok();
    });

    it('startJob with correct params when alwaysConnect_ is false', () => {
      resource.alwaysConnect_ = false;

      const stub = sinon.stub(resource, 'showResourcePopup');
      stub.resolves({ always: false, workerId: 'testId' });
      const fakeJob = { feature: 'CAMERA', localGumIsCallable: false };
      resource.startJob(fakeJob);
      stub.restore();
      expect(stub.calledOnceWith('CAMERA', false)).to.be.ok();
    });

    it('startJob with incorrect params when alwaysConnect_ is false', () => {
      resource.alwaysConnect_ = false;

      const stub = sinon.stub(resource, 'showResourcePopup');
      const spy = sinon.spy(resource, 'startJobImpl');
      const errorCallback = sinon.stub();
      const exception = {};
      stub.rejects(exception);
      const fakeJob = {
        feature: 'CAMERA',
        localGumIsCallable: false,
        errorCallback: errorCallback
      };
      resource.startJob(fakeJob);
      stub.restore();
      spy.restore();
      expect(stub.calledOnceWith('CAMERA', false)).to.be.ok();
    });
  });

  describe('#startJobImpl', () => {
    it('startJobImpl when alwaysConnect_ is localdevice', () => {
      resource.alwaysConnect_ = 'localdevice';

      const stub = sinon.stub();
      const fakeJob = {
        arguments: [{}],
        nativeGetUserMedia: stub
      };

      stub.resolves();
      resource.startJobImpl('testId', fakeJob);
      expect(stub.calledOnce).to.be.ok();
    });

    it('startJobImpl when workerId is localdevice', () => {
      resource.alwaysConnect_ = 'false';

      const stub = sinon.stub();
      const fakeJob = {
        arguments: [{}],
        nativeGetUserMedia: stub
      };

      stub.resolves();
      resource.startJobImpl('localdevice', fakeJob);
      expect(stub.calledOnce).to.be.ok();
    });

    it('check if startJobImpl works properly', () => {
      resource.alwaysConnect_ = 'false';

      const stub = sinon.stub(resource, 'requestService');
      const fakeJob = {};
      stub.resolves();
      resource.startJobImpl('testId', fakeJob);
      stub.restore();

      expect(stub.calledOnceWith('testId')).to.be.ok();
    });

    it('check if startJobImpl handles errors properly', () => {
      resource.alwaysConnect_ = 'false';

      const stub = sinon.stub(resource, 'requestService');
      const spy = sinon.spy();
      const fakeJob = { errorCallback: spy };
      stub.rejects();
      resource.startJobImpl('testId', fakeJob);
      stub.restore();
      expect(stub.calledOnceWith('testId')).to.be.ok();
    });
  });

  describe('#startJobWithExistingWorker', () => {
    it('check the job starts well if there is existing worker', () => {
      const func = sinon.fake();
      resource.worker_ = {
        startJob: func
      };
      resource.startJobWithExistingWorker({});
      resource.worker_ = null;
      expect(func.calledOnce).to.be.ok();
    });
  });

  describe('#stopJob', () => {
    it('check the job stops well if there is existing worker', () => {
      const func = sinon.fake();
      resource.worker_ = {
        stopJob: func
      };
      resource.stopJob('CAMERA');
      resource.worker_ = null;
      expect(func.calledOnceWith('CAMERA')).to.be.ok();
    });
  });
});
