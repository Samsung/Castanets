//To get loading time using node.js
//After opening chrome browser with option --remote-debugging-port=9222
//usage: node ./filename.js http://www.url.com
//
const CDP = require('chrome-remote-interface');
const fs = require('fs');
const util = require('util');
const path = require('path');

const g_duration = 20 * 1000;

var numRun = 1;
var args = process.argv.slice(2);

if(args.length < 1){
	console.log("Argument missing");
	console.log("Usage: node ./" + path.basename(__filename) + " http://www.url.com");
	process.exit(1);
}

function sleep(ms) {
	return new Promise(resolve => setTimeout(resolve, ms));
}

CDP(async (client) => {
	try {
		const {Page} = client;
		const loadEventFired = Page.loadEventFired();

		await Page.enable();
		for(i = 0; i < numRun; i++){
			start_time = new Date();
			await Page.navigate({url: args[0]});
			await new Promise((resolve, reject) => {
				const timeout = setTimeout(() => {
					console.log("Timeout: " + args[0]);
					Page.stopLoading();
					reject();
				}, g_duration);
	            loadEventFired.then(async () => {
					clearTimeout(timeout);
					Page.stopLoading();
					resolve();
				});
			});
			end_time = new Date();
			console.log(end_time.getTime() - start_time.getTime());
			await sleep(1500);
		}
	 } catch (err) {
		 console.error(err);
	 } finally {
		 await client.close();
	 }
	
}).on('error', (err) => {
	    console.error(err);
});
