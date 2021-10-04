import Logger from './logger';

const logger = new Logger('codec-helper.js');

export default class CodecHelper {
  static getPreferredCodecs(codecs) {
    if (!Array.isArray(codecs)) {
      codecs = [codecs];
    }
    let preferredCodecs = [];
    if (codecs.length > 0) {
      let supportedCodecs = RTCRtpSender.getCapabilities('video').codecs;
      for (const codecName of codecs) {
        supportedCodecs = supportedCodecs.filter(codec => {
          if (codec.mimeType.match(new RegExp(codecName, 'i'))) {
            preferredCodecs.push(codec);
            return false;
          }
          return true;
        });
      }
      preferredCodecs = preferredCodecs.concat(supportedCodecs);
      let codecOrder = '';
      for (const codec of preferredCodecs) {
        if (codecOrder.indexOf(codec.mimeType) === -1) {
          codecOrder += codec.mimeType + ', ';
        }
      }
      logger.info('codec order : ' + codecOrder);
    }
    return preferredCodecs;
  }
}
