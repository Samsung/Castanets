| Build Status | [![CircleCI](https://circleci.sec.samsung.net/gh/HighPerformanceWeb/offload.js/tree/master.svg?style=svg&circle-token=429261d64359bfb45426f9d5cd818e96e1823476)](https://circleci.sec.samsung.net/gh/HighPerformanceWeb/offload.js/tree/master) |
| ------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |


# Offload.js

Offload.js is a multi device Offloading Framework for Javascript. It can be used in web applications based on the needs of the developer,

- to offload any compute intensive JS functions to another powerful edge device in the network.
- to offload unsupported device specific apis (if any) used to access Camera (getUserMedia), HRM, gps etc, to another capable edge device to get benefited from.

## Main Components:

- Offload Server:

1. Offload server sits on the client device and processes the offloading requests between the client web application and the worker applications running on the edge nodes.
2. In compute offloading, the offload server receives the offload request from the client and serves back once the worker app finishes the processing.
3. Similarly in resource offloading, the offload server acts as a mediator between client and worker applications to transfer the camera stream, sensor data etc.

- Offload Worker:

1. Offload worker is a independent web application, which can process offloaded js computations or can share the camera stream data, sensor data to the client application.
2. First it registers itself with the Offload server with the particular device capability and receives offload request from the offload server to processing.

- Client Application:

1. Client application includes offload.js library to use the capability of worker edge devices like compute power, camera, sensors etc.

## Build offload.js

### Install node.js version 12 or higher

### Install npm dependecies

```
npm install
```

### Build offload.js

This creates js files named `offload.js` and `offload-worker.js` in `dist/`.
```sh
npm run build
```

[TIP] Automatically rebuilding offload.js when source changes using by the following command.
```sh
npm run watch:offload
```

## How to run Offload Worker and samples on Web Browser

#### Generate a self signed ceritificate
```
cd offload-server/src/
openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 365 -out cert.pem -subj "/CN=localhost"
cd -
```

#### Run Offload Server
The Offload Server is a web server that hosts offload-worker and several samples, and a signaling server for WebRTC P2P connection between the samples and offload-worker

```
node offload-server/src/app.js
```
* OffloadServer will listen on 5443 port for HTTPS and 9559 port for HTTP by default.

#### Run Offload Worker and sample client app on web browser

The offload worker will provide camera feature to client app running on device which does not have camera H/W.

##### offload worker

* URL :https://\<OffloadServer Address\>:5443/offload-worker
* The offload worker requires camera H/W and https protocol to access.

##### Sample

* URL : http://\<OffloadServer Address\>:9559/getusermedia/
* OffloadServer requires https protocol to access camera.

## How to use offload.js library in client app side

#### Offloading getUserMedia

- Add the offload.js to your project and use the `getUserMedia` as you normally use the `navigator.getUserMedia`.

e.g sample/tizen/roffdemo

Example:

```html
<head>
  <script src="offload.js"></script>
</head>
...
<script>
  getUserMedia({video: true, audio: false}, {
    ...
  });
</script>
```

#### Compute Offloading:

- Generate compute offload config from Static Analyzer.

##### static-analyzer.js

1. Install the dependent node modules

```sh
static-analyzer$ npm install
```

2. Run Static Analyzer script

```sh
static-analyzer$ node src/static-analyzer.js <JS files... or Directory containing all JS files...>
```

#### Client web application.

1. Add script file 'offload.js' to your head tag.
2. Use the json config output from static-analyser and use it as coff_config.json file.

```
<head>
    <script src="serialize_javascript.js"></script>
    <script src="offload.js"></script>
</head>
```

- Config file example

```
{
  "functions" : [
    {
      "id" : "offloadFoo",
      "globals" : [ "foo1", "foo2" ]
    },
    {
      "id" : "offloadBar",
      "globals" : [ "foo1", "bar1" ]
    }
  ]
}

```

- Original fucntion

```
function offloadBar(param) {
  console.log("offloadBar with: " + param);
}
```

- Rewritten function by offload.js

```
function offloadBar(param) {
  let ret;
  const args = Array.prototype.slice.call(arguments);
  if (offloadInfoMap["offloadBar_ei65iakwb"].elapsedTime > ELAPSED_TIME_THRESHOLD_MS) {
    ret = offloadTo(offloadBar_ei65iakwb, args);
  } else {
    const t0 = performance.now();
    ret = offloadInfoMap["offloadBar_ei65iakwb"].funcObject.
        apply(null, args);
    const t1 = performance.now();
    console.log("elapsed time for offloadBar: " + (t1 - t0));
    offloadInfoMap["offloadBar_ei65iakwb"].elapsedTime = t1 - t0;
  }
  return ret;
}
```

Observe the functions in the coff_config.json is offloaded if the local execution time of those functions is greater than the
predefined threshod time (3000ms).

## List of Maintainers

- Hunseop Jeong (hs85.jeong@samsung.com)
- Youngha Jung (yh106.jung@samsung.com)
- Insoon Kim (is46.kim@samsung.com)
- Dongjun Kim (djmix.kim@samsung.com)
- Jinwoo Jeong (jw00.jeong@samsung.com)
- Joonhun Park (jh718.park@samsung.com)
- Suyambulingam Rathinasamy Muthupandi (suyambu.rm@samsung.com )
- Aakash Pahuja (p.aakash@samsung.com)

## Governance

All decisions in this project are made by consensus, respecting the principles and rules of the community.  
Please refer to the [Samsung Inner Source Governance](https://github.sec.samsung.net/InnerSource/SamsungInnerSourceProgram/blob/master/GettingStarted/Governance.md) in more detail.
