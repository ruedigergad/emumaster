
function byteCountToText(count) {
	if (count < 1024)
		return count + " B"
	else if (count < 1024 * 1024)
		return (count / 1024) + " KB"
	else
		return (count / 1024 / 1024) + " MB"
}

function hexToString(hex) {
	var result = ""
	for (var i = 0; i < 8; i++) {
		var x = hex & 0xF
		hex = hex >> 4
		if (x >= 10)
			x = String.fromCharCode(65 + x - 10)
		result = x  + result
	}
	return "0x" + result
}
