window.onload = function() {
	// TODO:: Do your initialization job
	// add eventListener for tizenhwkey
	document.addEventListener('tizenhwkey', function(e) {
		if (e.keyName === "back") {
			try {
				tizen.application.getCurrentApplication().exit();
			} catch (ignore) {}
		}
	});
    

	var iframe = document.querySelector("iframe");
	var searchBtn = iframe.contentWindow.document.querySelector("#globalSearchButton");
	searchBtn.onclick = null;
	searchBtn.onclick = function() {
		QRScanner.initiate({
			onResult: result => {
<<<<<<< HEAD
				fetch("http://oapi.ssg.com/front/cart/save.ssg", {
					 "body": result,
						"method": "POST",
					}
				).then(() => {
=======
				var results = result.split(',');
				var data = {
					cartTypeCd: results[0],
					infloSiteNo: results[1],
					items: [
						{
							siteNo: results[2],
							itemId: results[3],
							uitemId: results[4],
							ordQty: results[5],
							salestrNo: results[6],
							hopeShppDt: ""
						}
					]
				}

				var body = JSON.stringify(data);

				fetch("http://oapi.ssg.com/front/cart/save.ssg", {
					"body": body,
					"method": "POST",
				}).then(() => {
>>>>>>> upstream/master
					location.href="https://pay.ssg.com/cart/dmsShpp.ssg?gnb=cart";
				});
			},
			timeout: 100000
		});
	}
};
