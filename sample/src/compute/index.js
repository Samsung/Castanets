let counter = 0, aCounter = 0, mixCounter = 0;

async function funcCaller(f, args) {
  setLoading(f, true);
  await funcCall(f, args);
  setLoading(f, false);
}

function setLoading(f, state) {
  console.log("SETLOADING: ", f, state);
  switch (f) {
    case "myFunc": {
      document.getElementById("simpleBtn").innerHTML = state ? "Loading..." : "Option 1";
      break;
    }
    case "bubbleSort": {
      document.getElementById("sortBtn").innerHTML = state ? "Loading..." : "Option 2";
      break;
    }
    case "findXYZ": {
      document.getElementById("xyzBtn").innerHTML = state ? "Loading..." : "Option 3";
      break;
    }
    case "hasDuplicates": {
      document.getElementById("dupsBtn").innerHTML = state ? "Loading..." : "Option 4";
      break;
    }
    case "powerset": {
      document.getElementById("powerBtn").innerHTML = state ? "Loading..." : "Option 5";
      break;
    }
  }
}

function funcCall(f, args) {
  const OUTPUT = document.getElementById("outputPanelResults"),
    TIME_TAKEN = document.getElementById("timeTaken");
  let start = new Date(),
    end = new Date();
  TIME_TAKEN.innerHTML = "...";
  OUTPUT.innerHTML = "...";
  start = new Date();
  let res = "";

  switch (f) {
    case "myFunc": {
      res = myFunc(args);
      writeResult(OUTPUT, res, start, TIME_TAKEN);
      break;
    }
    case "increaseCounter": {
      res = increaseCounter();
      writeResult(OUTPUT, res, start, TIME_TAKEN);
      break;
    }
    case "increaseMixedCounter": {
      res = increaseMixedCounter();
      writeResult(OUTPUT, res, start, TIME_TAKEN);
      break;
    }
    case "decreaseCounter": {
      res = decreaseCounter();
      writeResult(OUTPUT, res, start, TIME_TAKEN);
      break;
    }
    case "bubbleSort": {
      res = bubbleSort(args);
      writeResult(OUTPUT, res, start, TIME_TAKEN);
      break;
    }
    case "findXYZ": {
      findXYZ(args).then(res => writeResult(OUTPUT, res, start, TIME_TAKEN));
      break;
    }
    case "increaseCounterAsync": {
      increaseCounterAsync().then(res => writeResult(OUTPUT, res, start, TIME_TAKEN));
      break;
    }
    case "decreaseCounterAsync": {
      decreaseCounterAsync().then(res => writeResult(OUTPUT, res, start, TIME_TAKEN));
      break;
    }
    case "decreaseMixedCounter": {
      decreaseMixedCounter().then(res => writeResult(OUTPUT, res, start, TIME_TAKEN));
      break;
    }
    case "hasDuplicates": {
      res = hasDuplicates(args);
      writeResult(OUTPUT, res, start, TIME_TAKEN);
      break;
    }
    case "powerset": {
      res = powerset(args);
      writeResult(OUTPUT, res, start, TIME_TAKEN);
      break;
    }
    default:
      writeResult(OUTPUT, "Undef", start, TIME_TAKEN);
      break;
  }
}

function writeResult(out, res, start, time_taken) {
  out.innerHTML = res;
  end = new Date();
  time_taken.innerHTML = Number(new Date() - start) + " ms";
}

function myFunc(p) {
  return p;
}

function increaseCounter() {
  counter++;
  return "<center> Sync Counter Value: <br>" + counter + "</center>";
}

function decreaseCounter() {
  counter--;
  return "<center> Sync Counter Value: <br>" + counter + "</center>";
}

function increaseMixedCounter() {
  mixCounter++;
  return "<center> Mixed Counter Value increased: <br>" + mixCounter + "</center>";
}

async function decreaseMixedCounter() {
  mixCounter--;
  return "<center> Mixed Counter Value decreased: <br>" + mixCounter + "</center>";
}

function bubbleSort(upper) {
  let inputArr = [];
  for (let index = upper; index >= 0; index--) inputArr.push(index);
  let len = inputArr.length;
  for (let i = 0; i < len; i++) {
    for (let j = 0; j < len; j++) {
      if (inputArr[j] > inputArr[j + 1]) {
        let tmp = inputArr[j];
        inputArr[j] = inputArr[j + 1];
        inputArr[j + 1] = tmp;
      }
    }
  }
  return inputArr;
}

async function findXYZ(n) {
  const solutions = [];
  for (let x = 0; x < n; x++) {
    for (let y = 0; y < n; y++) {
      for (let z = 0; z < n; z++) {
        if (3 * x + 9 * y + 8 * z === 1179) {
          solutions.push({ x, y, z });
        }
      }
    }
  }
  return JSON.stringify(solutions);
}

async function increaseCounterAsync() {
  aCounter++;
  return new Promise((resolve, reject) => {
    resolve("<center> Async Counter Value: <br>" + aCounter + "</center>");
  });
}

async function decreaseCounterAsync() {
  aCounter--;
  return "<center> Async Counter Value: <br>" + aCounter + "</center>";
}

function hasDuplicates(upper) {
  let n = [];
  for (let index = upper; index >= 0; index--) n.push(index);
  let counter = 0;

  for (let outter = 0; outter < n.length; outter++) {
    for (let inner = 0; inner < n.length; inner++) {
      counter++;

      if (outter === inner) continue;

      if (n[outter] === n[inner]) {
        return true;
      }
    }
  }

  return `n: ${n.length}, counter: ${counter}`;
}

function powerset(upper) {
  let inputArr = [];
  for (let index = upper; index >= 0; index--)
    inputArr.push(index);
  let n = inputArr.length;
  for (let i = 1; i < n; i++) {
    // Choosing the first element in our unsorted subarray
    let current = inputArr[i];
    // The last element of our sorted subarray
    let j = i - 1;
    while ((j > -1) && (current < inputArr[j])) {
      inputArr[j + 1] = inputArr[j];
      j--;
    }
    inputArr[j + 1] = current;
  }
  return inputArr;
}
