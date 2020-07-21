function main(window, runtimeId) {
	var lastMessage = 0;
	var connected = false;
	var pen = {
		extension: true,
		name: "",
		isEraser: false,
		pressure: 0,
		get pointerType() { // 0 - out of proximity, 1 - pen, 2 - mouse, 3 eraser
			return (Date.now() - lastMessage) < 1000 ? 1 : 0;
		}
	};
	var error = null;

	var tablet = {
		gotPressure: null,
		error: function () {
			return error;
		},
		pen: function () {
			return connected ? pen : null;
		},
		focus: function () {
			if (port) {
				port.postMessage('focus');
			}
		},
		restart: function () {
			
		},
	};
	var port = chrome.runtime.connect(runtimeId);

	port.onDisconnect.addListener(function () {
		port = null;
	});

	port.onMessage.addListener(function (msg) {
		if (typeof msg.name !== 'undefined')
			pen.name = name;

		if (typeof msg.p !== 'undefined') {
			pen.pressure = msg.p;
			error = null;

			if (tablet.gotPressure)
				tablet.gotPressure(msg.p);
		}

		if (typeof msg.connected !== 'undefined') {
			connected = msg.connected;
			//console.log('connected', connected);
		}

		if (typeof msg.log !== 'undefined')
			console.log(msg.log);

		if (typeof msg.error !== 'undefined') {
			var errorMessage = msg.errorMessage || msg.error;
			if (!error || error.message !== errorMessage) {
				console.log('pressure plugin error:', errorMessage);
				error = {
					code: msg.error,
					message: errorMessage,
					installLink: msg.installLink,
				};
			}
		}

		lastMessage = Date.now();
	});

	port.postMessage('focus'); // TODO: only if focused ?

	window.addEventListener('focus', function () {
		if (port) {
			port.postMessage('focus');
		}
	});

	window.addEventListener('blur', function () {
		if (port) {
			port.postMessage('blur');
		}
	});

	window.Tablet = tablet;
}

var code = '(' + main.toString() + ')(window, "' + chrome.runtime.id + '");';
var script = document.createElement('script');
script.appendChild(document.createTextNode(code));
(document.body || document.head || document.documentElement).appendChild(script);
