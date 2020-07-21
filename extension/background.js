var nativeInstallUrl = 'https://github.com/Agamnentzar/chrome-stylus-pressure/releases/download/1.0.3/StylusPressurePlugin.msi';
var latestNativePluginVersion = '1.0.3';
var minimumNativePluginVersion = '1.0.0';
var nativePort = null;
var popupMessage = 'ok';
var popupError = null;
var timeout = null;
var ports = [];
var os = 'Unknown';
var connected = false;
var restart = function () {
	if (nativePort) {
		nativePort.disconnect();
		nativePort = null;
		timeout = setTimeout(connectNativePort, 500);
	}
};

if (navigator.appVersion.indexOf('Win') !== -1) os = 'Windows';
if (navigator.appVersion.indexOf('Mac') !== -1) os = 'Mac';
if (navigator.appVersion.indexOf('X11') !== -1) os = 'Linux'; // UNIX
if (navigator.appVersion.indexOf('Linux') !== -1) os = 'Linux';

function send(msg) {
	ports.forEach(function (port) {
		port.postMessage(msg);
	});
}

function log(msg) {
	send({ log: msg });
}

function updatePopup(icon, message, error) {
	ports.forEach(function (port) {
		chrome.pageAction.setIcon({ tabId: port.sender.tab.id, path: icon });
	});
	popupMessage = message;
	popupError = error;
}

function connectNativePort() {
	timeout = null;

	if (nativePort)
		return;

	try {
		connected = false;
		//log('connecting');
		nativePort = chrome.runtime.connectNative('com.agamnentzar.stylus');

		nativePort.onMessage.addListener(function (msg) {
			//log('msg: ' + JSON.stringify(msg));

			if (!connected) {
				connected = true;
				updatePopup('icon.png', 'ok', null);
				send({ connected: connected });
			}

			if (msg.version && minimumNativePluginVersion > msg.version) {
				updatePopup('error.png', 'update', null);
			} else if (msg.version && latestNativePluginVersion > msg.version) {
				updatePopup('warning.png', 'upgrade', null);
			}

			send(msg);
		});

		nativePort.onDisconnect.addListener(function () {
			//log('disconnected');
			var errorMessage = chrome.runtime.lastError.message;
			connected = false;
			updatePopup('error.png', 'install', errorMessage);
			send({
				error: 'install',
				errorMessage: errorMessage,
				installLink: nativeInstallUrl,
				connected: connected
			});
			timeout = setTimeout(connectNativePort, 1000);
			nativePort = null;
		});
	} catch (e) {
		connected = false;
		popupError = e.message;
		send({ error: 'unknown', errorMessage: e.message, connected: connected });
		timeout = setTimeout(connectNativePort, 1000);
	}
}

if (os === 'Windows' || os === 'Linux') {
	chrome.runtime.onConnectExternal.addListener(function (port) {
		chrome.pageAction.show(port.sender.tab.id);

		if (!timeout)
			connectNativePort();

		ports.push(port);

		port.onDisconnect.addListener(function () {
			ports.splice(ports.indexOf(port), 1);

			if (ports.length === 0 && nativePort) {
				nativePort.disconnect();
				nativePort = null;
			}
		});

		port.onMessage.addListener(function (msg) {
			if (msg === 'restart') restart();
			else if (nativePort) nativePort.postMessage(msg);
			//log('posting: ' + (nativePort ? 'true' : 'false') + ' ' + JSON.stringify(msg));
		});

		port.postMessage({ connected: connected });
	});
} else {
	popupMessage = 'notsupported';
}
