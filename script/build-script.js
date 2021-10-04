#!/usr/bin/env node
const { program } = require('commander');
const fse = require('fs-extra');
const exec = require('child_process').exec;
const rimraf = require('rimraf');
const path = require('path');
const glob = require('glob');
const pkg = require('../package.json');

const androidPath = path.resolve(__dirname, '../offload-worker/android');
const distPath = path.resolve(__dirname, '../dist');
const testcasesPath = path.resolve(__dirname, '../test');
const samplePath = path.resolve(__dirname, '../sample');

function buildAndroid() {
  return new Promise((resolve, reject) => {
    rimraf(distPath + '/*.apk', err => {
      if (err) {
        console.error('Fail to delete existing apk files');
        return reject(err);
      }

      exec(
        androidPath + '/gradlew build',
        {
          cwd: androidPath
        },
        (err, stdout, stderr) => {
          if (err) {
            console.error(stderr);
            console.error('Fail to build the project');
            return reject(err);
          }

          console.log(stdout);

          glob(androidPath + '/**/*.apk', (err, files) => {
            if (err) {
              console.error('Fail to find built apk files');
              return reject(err);
            }

            for (const file of files) {
              fse.copyFileSync(file, path.join(distPath, path.basename(file)));
            }

            return resolve();
          });
        }
      );
    });
  });
}

function buildTizenWeb(sourcePath) {
  return new Promise((resolve, reject) => {
    rimraf(sourcePath + '/.buildResult', err => {
      if (err) {
        console.error(`Fail to remove .buildResult: ${sourcePath}`);
        return reject(err);
      }

      exec(`tizen build-web -- ${sourcePath}`, (err, stdout, stderr) => {
        if (err) {
          console.error(stderr);
          console.error(`Fail to build the tizen project: ${sourcePath}`);
          return reject(err);
        }

        console.log(stdout);

        exec(
          `tizen package -t wgt -- ${sourcePath}/.buildResult`,
          (err, stdout, stderr) => {
            if (err) {
              console.error(stderr);
              console.error(
                `Fail to package the tizen web project: ${sourcePath}`
              );
              return reject(err);
            }

            console.log(stdout);

            glob(sourcePath + '/.*/*.wgt', (err, files) => {
              if (err) {
                console.error('Fail to find built wgt files');
                return reject(err);
              }

              for (const file of files) {
                fse.copyFileSync(
                  file,
                  path.join(distPath, path.basename(file).replace(/\s+/g, ''))
                );
              }

              return resolve();
            });
          }
        );
      });
    });
  });
}

async function buildTizen() {
  // Signaling Server
  const serverPath = path.resolve(__dirname, '../offload-server');

  await fse.copyFile(
    path.join(distPath, 'offload.js'),
    path.join(serverPath, 'tizen', 'service', 'gen', 'public', 'offload.js')
  );
  await fse.copy(
    path.join(serverPath, 'src'),
    path.join(serverPath, 'tizen', 'service', 'gen')
  );

  await buildTizenWeb(path.join(serverPath, 'tizen'));

  // Offload Worker
  const OffloadWorkerPath = path.resolve(__dirname, '../offload-worker/tizen');

  await buildTizenWeb(OffloadWorkerPath);
}

async function buildTest() {
  const testNames = [
    'offloadMediaDevicesTC',
    'offloadTizenPPMTC',
    'offloadTizenHAMTC'
  ];

  for (const testName of testNames) {
    const testPath = path.resolve(testcasesPath, 'tizen', testName);

    await buildTizenWeb(testPath);
  }
}

async function buildSample() {
  const ROFFPath = path.resolve(samplePath, 'tizen', 'roffdemo');
  const SSGPath = path.resolve(samplePath, 'tizen', 'SSG');
  await fse.copyFile(
    path.join(distPath, 'offload.js'),
    path.join(SSGPath, 'js', 'offload.js')
  );

  await buildTizenWeb(ROFFPath);
  await buildTizenWeb(SSGPath);
}

async function buildAll() {
  await buildAndroid();
  await buildTizen();
  await buildTest();
  await buildSample();
}

program
  .option('-a, --android', 'build anroid packages', buildAndroid)
  .option('-t, --tizen', 'build tizen packages', buildTizen)
  .option('-e, --test', 'build test packages', buildTest)
  .option('-s, --sample', 'build sample packages', buildSample)
  .option('--all', 'build all packages', buildAll);

program.version(pkg.version, '-v, --version', 'output the version number');
program.parse(process.argv);

if (process.argv.length === 2) {
  program.help();
}
