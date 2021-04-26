import { sendWasmModule } from "./helpers.mjs";

window.onmessage = async (e) => {
  // These could come from the parent or siblings.
  if (e.data.constructor === WebAssembly.Module) {
    e.source.postMessage("WebAssembly.Module message received", "*");
  }

  // These only come from the parent.
  if (e.data.command === "set document.domain") {
    document.domain = e.data.newDocumentDomain;
    parent.postMessage("document.domain is set", "*");
  } else if (e.data.command === "send WASM module") {
    const destinationFrameWindow = parent.frames[e.data.indexIntoParentFrameOfDestination];
    const whatHappened = await sendWasmModule(destinationFrameWindow);
    parent.postMessage(whatHappened, "*");
  } else if (e.data.command === "access document") {
    const destinationFrameWindow = parent.frames[e.data.indexIntoParentFrameOfDestination];
    try {
      destinationFrameWindow.document;
      parent.postMessage("accessed document successfully", "*");
    } catch (e) {
      parent.postMessage(e.name, "*");
    }
  } else if (e.data.command === "access location.href") {
    const destinationFrameWindow = parent.frames[e.data.indexIntoParentFrameOfDestination];
    try {
      destinationFrameWindow.location.href;
      parent.postMessage("accessed location.href successfully", "*");
    } catch (e) {
      parent.postMessage(e.name, "*");
    }
  } else if (e.data.command === "get originIsolationRestricted") {
    parent.postMessage(self.originIsolationRestricted, "*");
  }

  // We could also receive e.data === "WebAssembly.Module message received",
  // but that's handled by await sendWasmModule() above.
};

window.onmessageerror = e => {
  e.source.postMessage("messageerror", "*");
};

document.body.textContent = location.href;
