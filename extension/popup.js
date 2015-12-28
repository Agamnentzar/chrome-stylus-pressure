document.addEventListener('DOMContentLoaded', function () {
	var message = chrome.extension.getBackgroundPage().popupMessage;
	var error = chrome.extension.getBackgroundPage().popupError;
	var errorDiv = document.getElementById('message-' + message);

	document.getElementById('restart').onclick = chrome.extension.getBackgroundPage().restart;

	if (errorDiv)
		errorDiv.style.display = 'block';
	else
		document.getElementById('error').innerHTML = error;
});
