import QtQuick 2.0
import "../base"

EMSwitchOption {
	property string optionName
	onCheckedChanged: diskGallery.setGlobalOption(optionName, checked)
	Component.onCompleted: checked = diskGallery.globalOption(optionName)
}
