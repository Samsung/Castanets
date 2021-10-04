import { Offload } from '~/offload';

describe('offload.js', function () {
  let offload;

  before(function () {
    offload = new Offload();
  });

  describe('constructor', function () {
    it('initialize resources when creating the offload', function () {
      // resources: gum, gyro, ham
      expect(offload.resources).to.have.length(3);
    });
  });

  describe('#connect', function () {
    it('get a correct ServerUrl without address', async function () {
      const serverUrl = await offload.connect();
      expect(serverUrl).equal('http://localhost:9559/offload-js');
    });

    it('connect to server without appendix offload-js', async function () {
      const serverUrl = await offload.connect('http://localhost:9559');
      expect(serverUrl).equal('http://localhost:9559/offload-js');
    });

    it('connect to server with appendix offload-js', async function () {
      const serverUrl = await offload.connect(
        'http://localhost:9559/offload-js'
      );
      expect(serverUrl).equal('http://localhost:9559/offload-js');
    });
  });
});
