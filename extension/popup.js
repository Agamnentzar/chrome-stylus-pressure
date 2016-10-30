document.addEventListener('DOMContentLoaded', function () {
	var message = chrome.extension.getBackgroundPage().popupMessage;
	var error = chrome.extension.getBackgroundPage().popupError;
	var installUrl = chrome.extension.getBackgroundPage().nativeInstallUrl;
	var errorDiv = document.getElementById('message-' + message);
	var links = document.querySelectorAll('.install-link');

	for (var i = 0; i < links.length; i++)
		links.item(i).href = installUrl;

	document.getElementById('restart').onclick = chrome.extension.getBackgroundPage().restart;

	if (errorDiv)
		errorDiv.style.display = 'block';
	else
		document.getElementById('error').innerHTML = error;
});
