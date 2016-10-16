document.addEventListener('DOMContentLoaded', function () {
	var message = chrome.extension.getBackgroundPage().popupMessage;
	var error = chrome.extension.getBackgroundPage().popupError;
	var installUrl = chrome.extension.getBackgroundPage().nativeInstallUrl;
	var errorDiv = document.getElementById('message-' + message);

	document.getElementById('restart').onclick = chrome.extension.getBackgroundPage().restart;
	document.getElementById('install-link-1').href = installUrl;
	document.getElementById('install-link-2').href = installUrl;

	if (errorDiv)
		errorDiv.style.display = 'block';
	else
		document.getElementById('error').innerHTML = error;
});
