# 代码截取并修改自esh，所有权和许可证由esh项目保留
if ($Host.UI.SupportsVirtualTerminal) {
	function Escape { [char]27 + '[' + $args }
	$VirtualTerminal = @{
		Escape          = Escape
		Colors          = @{
			Black         = Escape '30m'
			Red           = Escape '31m'
			Green         = Escape '32m'
			Yellow        = Escape '33m'
			Blue          = Escape '34m'
			Magenta       = Escape '35m'
			Cyan          = Escape '36m'
			White         = Escape '37m'
			Default       = Escape '39m'
			BrightBlack   = Escape '90m'
			BrightRed     = Escape '91m'
			BrightGreen   = Escape '92m'
			BrightYellow  = Escape '93m'
			BrightBlue    = Escape '94m'
			BrightMagenta = Escape '95m'
			BrightCyan    = Escape '96m'
			BrightWhite   = Escape '97m'
			Reset         = Escape '39m'
		}
		Styles          = @{
			Italic      = Escape '3m'
			Underline   = Escape '4m'
			Blink       = Escape '5m'
			Reverse     = Escape '7m'
			Hide        = Escape '8m'
			NoItalic    = Escape '23m'
			NoUnderline = Escape '24m'
			NoBlink     = Escape '25m'
			NoReverse   = Escape '27m'
			NoHide      = Escape '28m'
			Reset       = Escape '23m'
		}
		ResetAll        = Escape '0m'
		ResetColors     = Escape '39m'
		ResetStyles     = Escape '23m'

		#保存当前光标位置
		SaveCursor      = Escape 's'
		#恢复光标位置
		RestoreCursor   = Escape 'u'
		#清除从光标到行尾的内容
		ClearLine       = Escape 'K'
		#清除从光标到行首的内容
		ClearLineLeft   = Escape '1K'
		#清除整行
		ClearLineAll    = Escape '2K'
		#清除从光标到屏幕底部的内容
		ClearScreenDown = Escape 'J'
		#清除从屏幕顶部到光标的内容
		ClearScreenUp   = Escape '1J'
		#清除整屏
		ClearScreenAll  = Escape '2J'
	}
	Remove-Item function:Escape
}
