import { Worker } from '~/offload/core/worker';

describe('core/worker.js', () => {
  let worker;

  beforeEach(() => {
    worker = new Worker('workerId');
  });

  describe('constructor', () => {
    it('check the initial values of worker members', () => {
      const stub = sinon.stub(Worker.prototype, 'setUpPeerConnection_');
      stub.returns();
      worker = new Worker('workerId');

      expect(worker.workerId_).to.be.a('string');
      expect(worker.workerId_).to.eql('workerId');
      expect(worker.clientId_).to.be.a('string');
      expect(worker.deviceName_).to.be.a('string');
      expect(worker.peerConnection_).to.eql(null);
      expect(worker.dataChannel_).to.eql(null);
      expect(worker.isConnected_).to.eql(false);
      expect(worker.runningJobs_).to.be.a(Map);
      expect(worker.pendingJobs_).to.be.a(Map);
      expect(worker.gumStream_).to.eql(null);
      expect(worker.gumCapabilities_).to.eql(null);
      expect(worker.gumSettings_).to.eql(null);
      expect(worker.gumConstraints_).to.eql(null);
      expect(worker.applyConstraintsResolve_).to.eql(null);
      expect(worker.applyConstraintsReject_).to.eql(null);
      expect(worker.nativeApplyConstraints_).to.eql(null);

      stub.restore();
      expect(stub.calledOnce).to.be.ok();
    });
  });

  describe('#setUpPeerConnection_', () => {
    it('check if the logic in setUpPeerConnection_ is executed correctly', () => {
      const stub = sinon.stub(RTCPeerConnection.prototype, 'createOffer');
      stub.resolves();

      worker.setUpPeerConnection_();
      expect(worker.peerConnection_).to.be.a(RTCPeerConnection);
      expect(worker.peerConnection_.onicecandidate).to.not.eql(null);
      expect(worker.peerConnection_.ontrack).to.not.eql(null);

      expect(worker.dataChannel_).to.not.eql(null);
      expect(worker.dataChannel_.onopen).to.not.eql(null);
      expect(worker.dataChannel_.onclose).to.not.eql(null);
      expect(worker.dataChannel_.onmessage).to.not.eql(null);

      stub.restore();
      expect(stub.calledOnce).to.be.ok();
    });
  });

  describe('#handleIceCandidateEvent_', () => {
    it('check whether the message is properly sent if there is an event candidate', () => {
      const spy = sinon.spy(worker, 'sendSignalMessage_');
      worker.handleIceCandidateEvent_({
        candidate: {}
      });
      spy.restore();
      expect(spy.calledOnce).to.be.ok();
    });
  });

  describe('#handleTrackEvent_', () => {
    it('check the operation if there is more than 1 video track', () => {
      const stub1 = sinon.stub(worker, 'updateMediaStreamTrack_');
      const stub2 = sinon.stub(worker, 'getRunningJob_');
      const track = {};
      const stream = {
        getVideoTracks: function () {
          return [track];
        },
        getTracks: () => [{}]
      };
      worker.handleTrackEvent_({
        streams: [stream]
      });

      stub1.restore();
      stub2.restore();

      expect(stub1.calledOnceWith(track)).to.be.ok();
      expect(stub2.calledOnceWith('CAMERA')).to.be.ok();
    });

    it('check the operation if there is more than 1 audio track', () => {
      const stub = sinon.stub(worker, 'getRunningJob_');
      const stream = {
        getVideoTracks: () => [],
        getAudioTracks: () => [{}],
        getTracks: () => [{}]
      };
      worker.handleTrackEvent_({
        streams: [stream]
      });
      stub.restore();
      expect(stub.calledOnceWith('MIC')).to.be.ok();
    });
  });

  describe('#updateMediaStreamTrack_', () => {
    it('check if the functions necessary for the track are properly updated', () => {
      const capabilites = {};
      const settings = {};
      const track = { applyConstraints: function () {} };
      const constraints = {};
      const stub = sinon.stub(worker, 'sendPeerMessage_');
      worker.gumCapabilities_ = capabilites;
      worker.gumSettings_ = settings;
      worker.updateMediaStreamTrack_(track);
      expect(track.getCapabilities()).to.eql(capabilites);
      expect(track.getSettings()).to.eql(settings);
      track.applyConstraints(constraints);
      stub.restore();
      expect(
        stub.calledOnceWith({
          type: 'applyConstraints',
          feature: 'CAMERA',
          constraints: constraints
        })
      ).to.be.ok();
    });

    it('check if applyConstraints is invoked correctly when the constraints has the tizenAiAutoZoom property', () => {
      const constraints = { tizenAiAutoZoomTarget: { exact: 'face' } };
      const fake = sinon.fake();
      const track = { applyConstraints: fake };
      worker.updateMediaStreamTrack_(track);
      track.applyConstraints(constraints);
      expect(fake.calledOnceWith(constraints)).to.be.ok();
    });
  });

  describe('#handleDataChannelOpenEvent_', () => {
    it('check if the data channel is properly handled when it is opened', () => {
      const stub = sinon.stub(worker, 'startJob');
      const job = {};
      worker.pendingJobs_.set('test', job);
      worker.isConnected_ = false;
      worker.handleDataChannelOpenEvent_();
      stub.restore();
      expect(worker.isConnected_).to.eql(true);
      expect(stub.calledOnceWith(job)).to.be.ok();
      expect(worker.pendingJobs_.size).to.eql(0);
    });
  });

  describe('#handleDataChannelCloseEvent_', () => {
    it('check if the data channel is properly handled when it is closed', () => {
      worker.isConnected_ = true;
      const stub = sinon.stub(worker, 'closePeerConnection_');
      worker.handleDataChannelCloseEvent_();
      expect(worker.isConnected_).to.eql(false);
      expect(worker.dataChannel_).to.eql(null);
      stub.restore();
      expect(stub.calledOnce).to.be.ok();
    });
  });

  describe('#closePeerConnection_', () => {
    it('check if peerConnection is properly closed', () => {
      const stub = sinon.stub(worker.peerConnection_, 'close');
      worker.closePeerConnection_();
      stub.restore();
      expect(stub.calledOnce).to.be.ok();
      expect(worker.peerConnection_).to.eql(null);
    });
  });

  describe('#handleDataChannelMessageEvent_', () => {
    it('check whether the error is properly handled if there is no running job', () => {
      const spy = sinon.spy(worker, 'getRunningJob_');
      const event = { data: '{"feature": "test"}' };
      worker.handleDataChannelMessageEvent_(event);
      spy.restore();
      expect(spy.returnValues[0]).to.eql(null);
    });

    it('check whether it works properly if the message type is data and feature is CAMERA', () => {
      const job = {};
      worker.runningJobs_.set('CAMERA', job);
      const event = {
        data:
          '{"feature": "CAMERA", "type": "data", "data": { "capabilities": "test", "settings": "test" }}'
      };
      worker.handleDataChannelMessageEvent_(event);
      expect(worker.gumCapabilities_).to.eql('test');
      expect(worker.gumSettings_).to.eql('test');
    });

    it('check whether it works properly if the message type is data and feature is CAMERA', () => {
      const job = {};
      worker.runningJobs_.set('COMPUTE', job);
      const event = {
        data:
          '{"feature": "COMPUTE", "type": "data", "data": { "offloadId": "test", "result": "test" }}'
      };
      // worker.handleDataChannelMessageEvent_(event);
    });

    it('check whether it works properly if the message type is data and feature is wrong', () => {
      const spy = sinon.spy();
      const job = { successCallback: spy };
      worker.runningJobs_.set('else', job);
      const event = { data: '{"feature": "else", "type": "data"}' };
      worker.handleDataChannelMessageEvent_(event);
      expect(spy.calledOnce).to.be.ok();
    });

    it('check whether it works properly if the message type is error', () => {
      const callback = sinon.spy();
      const job = { errorCallback: callback };
      const stub = sinon.stub(worker, 'stopJob');
      worker.runningJobs_.set('test', job);
      const event = {
        data:
          '{"feature": "test", "type": "error", "error": {"message": "test", "name": "test"}}'
      };
      worker.handleDataChannelMessageEvent_(event);
      stub.restore();
      expect(callback.calledOnce).to.be.ok();
      expect(stub.calledOnceWith('test')).to.be.ok();
    });

    it('check whether it works properly if the message type is applyConstraints and result is success', () => {
      const job = {};
      worker.runningJobs_.set('test', job);
      const spy = sinon.spy();
      worker.applyConstraintsResolve_ = spy;
      const event = {
        data:
          '{"feature": "test", "type": "applyConstraints", "result": "success", "data": { "settings": "test", "constraints": "test" }}'
      };
      worker.handleDataChannelMessageEvent_(event);
      expect(worker.gumSettings_).to.eql('test');
      expect(worker.gumConstraints_).to.eql('test');
      expect(spy.calledOnce).to.be.ok();
    });

    it('check whether it works properly if the message type is applyConstraints and result is wrong', () => {
      const job = {};
      worker.runningJobs_.set('test', job);
      const spy = sinon.spy();
      worker.applyConstraintsReject_ = spy;
      const event = {
        data:
          '{"feature": "test", "type": "applyConstraints", "result": "error", "error": {"message": "test", "name": "test"}}'
      };
      worker.handleDataChannelMessageEvent_(event);
      expect(spy.calledOnce).to.be.ok();
    });
  });

  describe('#getRunningJob_', () => {
    it('check whether null is returned if there is no running job', () => {
      const job = worker.getRunningJob_('test');
      expect(job).to.eql(null);
    });

    it('check whether the running job returned if there is the running job', () => {
      const obj = {};
      worker.runningJobs_.set('test', obj);
      const job = worker.getRunningJob_('test');
      expect(job).to.eql(obj);
    });
  });

  describe('#sendPeerMessage_', () => {
    it('check whether the send of the dataChannel_ is called properly', () => {
      const message = { msg: 'test' };
      worker.isConnected_ = true;
      const stub = sinon.stub(worker.dataChannel_, 'send');
      worker.sendPeerMessage_(message);
      expect(stub.calledOnceWith(JSON.stringify(message))).to.be.ok();

      worker.isConnected_ = false;
      worker.sendPeerMessage_(message);
      stub.restore();
      expect(stub.calledOnce).to.be.ok();
    });
  });

  describe('#startJob', () => {
    it('check if the sendPeerMessage_ is called properly when the worker is connected', () => {
      worker.isConnected_ = true;
      const stub = sinon.stub(worker, 'sendPeerMessage_');
      const spy = sinon.spy();
      worker.startJob({
        feature: 'test',
        arguments: [],
        resolver: spy
      });
      stub.restore();
      expect(stub.calledOnce).to.be.ok();
      expect(worker.runningJobs_.size).to.eql(1);
      expect(spy.calledOnce).to.be.ok();
    });

    it('check if the job is pending correctly when the worker is not connected', () => {
      worker.isConnected_ = false;
      worker.startJob({
        feature: 'test'
      });
      expect(worker.pendingJobs_.size).to.eql(1);
    });
  });

  describe('#stopJob', () => {
    it('check if the job stops properly when there is the running job', () => {
      const obj = {};
      worker.runningJobs_.set('test', obj);
      const stub = sinon.stub(worker, 'sendPeerMessage_');
      worker.stopJob('test');
      stub.restore();

      expect(stub.calledOnce).to.be.ok();
      expect(worker.runningJobs_.has('test')).to.eql(false);
      expect(worker.runningJobs_.size).to.eql(0);
    });

    it('check if the job deletes properly when there is the pending job', () => {
      const obj = {};
      worker.pendingJobs_.set('test', obj);
      worker.stopJob('test');
      expect(worker.pendingJobs_.has('test')).to.eql(false);
      expect(worker.pendingJobs_.size).to.eql(0);
    });
  });

  describe('#handleMessage', () => {
    it('check if the message is handled properly when the message type is offer', () => {
      const message = { type: 'offer' };
      const stub = sinon.stub(worker, 'handleOffer_');
      worker.handleMessage(message);
      stub.restore();
      expect(stub.calledOnceWith(message)).to.be.ok();
    });

    it('check if the message is handled properly when the message type is answer', () => {
      const message = { type: 'answer' };
      const stub = sinon.stub(worker, 'handleAnswer_');
      worker.handleMessage(message);
      stub.restore();
      expect(stub.calledOnceWith(message)).to.be.ok();
    });

    it('check if the message is handled properly when the message type is candidate', () => {
      const message = { type: 'candidate', candidate: {} };
      const stub = sinon.stub(worker, 'handleCandidate_');
      worker.handleMessage(message);
      stub.restore();
      expect(stub.calledOnceWith(message.candidate)).to.be.ok();
    });
  });

  describe('#handleOffer_', () => {
    it('check if the offer is handled properly', () => {
      const offer = {};
      const stub = sinon.stub();
      stub.withArgs(offer).resolves();

      worker.peerConnection_ = {
        setRemoteDescription: stub,
        createAnswer: stub,
        setLocalDescription: stub
      };

      worker.handleOffer_(offer);

      expect(stub.calledOnce).to.be.ok();
    });

    it('check if the errors are handled properly', () => {
      const offer = {};
      const stub = sinon.stub(worker.peerConnection_, 'setRemoteDescription');
      stub.withArgs(offer).rejects();

      worker.handleOffer_(offer);
      expect(stub.calledOnce).to.be.ok();
    });
  });

  describe('#handleAnswer_', () => {
    it('check if the answer is handled properly', () => {
      const answer = {};
      const stub = sinon.stub(worker.peerConnection_, 'setRemoteDescription');
      stub.withArgs(answer).rejects();
      worker.handleAnswer_(answer);
      stub.restore();
      expect(stub.calledOnceWith(answer)).to.be.ok();
    });
  });

  describe('#handleCandidate_', () => {
    it('check if the candidate is handled properly', () => {
      const candidate = {};
      const stub = sinon.stub(worker.peerConnection_, 'addIceCandidate');
      stub.withArgs(candidate).rejects();
      worker.handleCandidate_(candidate);
      stub.restore();
      expect(stub.calledOnceWith(candidate)).to.be.ok();
    });
  });
});
