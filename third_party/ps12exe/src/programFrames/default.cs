// 由 Ingo Karstein 创建的简单 PowerShell 宿主 (http://blog.karstein-consulting.com)
// 由 Markus Scholtes 重构并添加 GUI 支持

using System;
using System.Collections.Generic;
using System.Text;
using System.Management.Automation;
using System.Management.Automation.Runspaces;
using System.IO;
#if !Pwsh20
	using System.Management.Automation.Language;
#endif
using System.Globalization;
using System.Management.Automation.Host;
using System.Security;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Runtime.InteropServices;
#if noConsole
	using System.Windows.Forms;
	using System.Drawing;
#endif
using System.Runtime.Versioning;

// 不显示在属性对话框的详细信息选项卡中，但会嵌入到文件里
#if Resources
	[assembly: AssemblyDescription("$description")]
	[assembly: AssemblyCompany("$company")]
	[assembly: AssemblyTitle("$title")]
	[assembly: AssemblyProduct("$product")]
	[assembly: AssemblyCopyright("$copyright")]
	[assembly: AssemblyTrademark("$trademark")]
#endif
#if version
	[assembly: AssemblyVersion("$version")]
	[assembly: AssemblyFileVersion("$version")]
#endif
#if winFormsDPIAware
	[assembly: TargetFrameworkAttribute("$TargetFramework,Profile=Client")]
#endif

namespace PSRunnerNS {
	#if noConsole || credentialGUI
	internal class Credential_Form {
		[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
		private struct CREDUI_INFO {
			public int cbSize;
			public IntPtr hwndParent;
			public string pszMessageText;
			public string pszCaptionText;
			public IntPtr hbmBanner;
		}

		[Flags]
		enum CREDUI_FLAGS {
			INCORRECT_PASSWORD = 0x1,
			DO_NOT_PERSIST = 0x2,
			REQUEST_ADMINISTRATOR = 0x4,
			EXCLUDE_CERTIFICATES = 0x8,
			REQUIRE_CERTIFICATE = 0x10,
			SHOW_SAVE_CHECK_BOX = 0x40,
			ALWAYS_SHOW_UI = 0x80,
			REQUIRE_SMARTCARD = 0x100,
			PASSWORD_ONLY_OK = 0x200,
			VALIDATE_USERNAME = 0x400,
			COMPLETE_USERNAME = 0x800,
			PERSIST = 0x1000,
			SERVER_CREDENTIAL = 0x4000,
			EXPECT_CONFIRMATION = 0x20000,
			GENERIC_CREDENTIALS = 0x40000,
			USERNAME_TARGET_CREDENTIALS = 0x80000,
			KEEP_USERNAME = 0x100000,
		}

		public enum CredUI_ReturnCodes {
			NO_ERROR = 0,
			ERROR_CANCELLED = 1223,
			ERROR_NO_SUCH_LOGON_SESSION = 1312,
			ERROR_NOT_FOUND = 1168,
			ERROR_INVALID_ACCOUNT_NAME = 1315,
			ERROR_INSUFFICIENT_BUFFER = 122,
			ERROR_INVALID_PARAMETER = 87,
			ERROR_INVALID_FLAGS = 1004,
		}

		[DllImport("credui", CharSet = CharSet.Unicode)]
		private static extern CredUI_ReturnCodes CredUIPromptForCredentials(ref CREDUI_INFO credinfo,
			string targetName,
			IntPtr reserved1,
			int iError,
			StringBuilder userName,
			int maxUserName,
			StringBuilder password,
			int maxPassword,
			[MarshalAs(UnmanagedType.Bool)] ref bool pfSave,
			CREDUI_FLAGS flags);

		public class User_Pwd {
			public string User = string.Empty;
			public string Password = string.Empty;
			public string Domain = string.Empty;
		}

		internal static User_Pwd PromptForPassword(string caption, string message, string target, string user, PSCredentialTypes credTypes, PSCredentialUIOptions options) {
			// 初始化标志和变量
			StringBuilder userPassword = new StringBuilder("", 128), userID = new StringBuilder(user, 128);
			CREDUI_INFO credUI = new CREDUI_INFO();
			if (!string.IsNullOrEmpty(message)) credUI.pszMessageText = message;
			if (!string.IsNullOrEmpty(caption)) credUI.pszCaptionText = caption;
			credUI.cbSize = Marshal.SizeOf(credUI);
			bool save = false;

			CREDUI_FLAGS flags = CREDUI_FLAGS.DO_NOT_PERSIST;
			if ((credTypes & PSCredentialTypes.Generic) == PSCredentialTypes.Generic) {
				flags |= CREDUI_FLAGS.GENERIC_CREDENTIALS;
				if ((options & PSCredentialUIOptions.AlwaysPrompt) == PSCredentialUIOptions.AlwaysPrompt) {
					flags |= CREDUI_FLAGS.ALWAYS_SHOW_UI;
				}
			}

			// 以图形提示向用户询问密码
			CredUI_ReturnCodes returnCode = CredUIPromptForCredentials(ref credUI, target, IntPtr.Zero, 0, userID, 128, userPassword, 128, ref save, flags);

			if (returnCode == CredUI_ReturnCodes.NO_ERROR) {
				User_Pwd ret = new User_Pwd();
				ret.User = userID.ToString();
				ret.Password = userPassword.ToString();
				ret.Domain = "";
				return ret;
			}

			return null;
		}
	}
	#endif

	internal class PSRunnerRawUI: PSHostRawUserInterface {
		#if noConsole
			// GUI 输出时的控制台颜色会被读取和设置，但目前尚未使用（供将来使用）
			private ConsoleColor _GUIBackgroundColor = ConsoleColor.White;
			private ConsoleColor _GUIForegroundColor = ConsoleColor.Black;

			#if noConsole
			private string _windowTitleData;
			public PSRunnerRawUI() {
				// 加载 assembly:AssemblyTitle
				AssemblyTitleAttribute titleAttribute = (AssemblyTitleAttribute) Attribute.GetCustomAttribute(Assembly.GetExecutingAssembly(), typeof(AssemblyTitleAttribute));
				if (titleAttribute != null)
					_windowTitleData = titleAttribute.Title;
				else
					_windowTitleData = System.AppDomain.CurrentDomain.FriendlyName;
			}
			#endif
		#else
			const int STD_OUTPUT_HANDLE = -11;

			//CHAR_INFO 结构体早年是一个 union，因此我们用 LayoutKind.Explicit 尽量贴近它
			[StructLayout(LayoutKind.Explicit)]
			public struct CHAR_INFO {
				[FieldOffset(0)]
				internal char UnicodeChar;
				[FieldOffset(0)]
				internal char AsciiChar;
				[FieldOffset(2)] //2 字节似乎能正常工作
				internal UInt16 Attributes;
			}

			//COORD 结构体
			[StructLayout(LayoutKind.Sequential)]
			public struct COORD {
				public short X;
				public short Y;
			}

			//SMALL_RECT 结构体
			[StructLayout(LayoutKind.Sequential)]
			public struct SMALL_RECT {
				public short Left;
				public short Top;
				public short Right;
				public short Bottom;
			}

			/* 从控制台屏幕缓冲区的矩形字符单元格块读取字符与颜色属性数据，并把数据写入目标缓冲区指定位置的矩形块。 */
			[DllImport("Kernel32.dll", EntryPoint = "ReadConsoleOutputW", CharSet = CharSet.Unicode, SetLastError = true)]
			internal static extern bool ReadConsoleOutput(
				IntPtr hConsoleOutput,
				/* 该指针被视为 CHAR_INFO 结构二维数组的原点，数组大小由 dwBufferSize 参数指定。*/
				[MarshalAs(UnmanagedType.LPArray), Out] CHAR_INFO[, ] lpBuffer,
				COORD dwBufferSize,
				COORD dwBufferCoord,
				ref SMALL_RECT lpReadRegion);

			/* 把字符与颜色属性数据写入控制台屏幕缓冲区中指定的矩形字符单元格块。要写入的数据取自源缓冲区指定位置的相应大小矩形块。 */
			[DllImport("Kernel32.dll", EntryPoint = "WriteConsoleOutputW", CharSet = CharSet.Unicode, SetLastError = true)]
			internal static extern bool WriteConsoleOutput(
				IntPtr hConsoleOutput,
				/* 该指针被视为 CHAR_INFO 结构二维数组的原点，数组大小由 dwBufferSize 参数指定。*/
				[MarshalAs(UnmanagedType.LPArray), In] CHAR_INFO[, ] lpBuffer,
				COORD dwBufferSize,
				COORD dwBufferCoord,
				ref SMALL_RECT lpWriteRegion);

			/* 在屏幕缓冲区中移动一块数据。移动效果可用裁剪矩形加以限制，裁剪矩形之外的屏幕缓冲区内容保持不变。 */
			[DllImport("Kernel32.dll", SetLastError = true)]
			static extern bool ScrollConsoleScreenBuffer(
				IntPtr hConsoleOutput,
				[In] ref SMALL_RECT lpScrollRectangle,
				[In] ref SMALL_RECT lpClipRectangle,
				COORD dwDestinationOrigin,
				[In] ref CHAR_INFO lpFill);

			[DllImport("Kernel32.dll", SetLastError = true)]
			static extern IntPtr GetStdHandle(int nStdHandle);
		#endif

		public override ConsoleColor BackgroundColor {
			#if !noConsole
				get { return Console.BackgroundColor; }
				set { Console.BackgroundColor = value; }
			#else
				get { return _GUIBackgroundColor; }
				set { _GUIBackgroundColor = value; }
			#endif
		}

		public override System.Management.Automation.Host.Size BufferSize {
			get {
				#if !noConsole
					if (!Console_Info.IsOutputRedirected())
						return new System.Management.Automation.Host.Size(Console.BufferWidth, Console.BufferHeight);
				#endif
				// 返回默认值。如果没有返回有效值，WriteLine 将不会被调用
				return new System.Management.Automation.Host.Size(120, 50);
			}
			set {
				#if !noConsole
					Console.BufferWidth = value.Width;
					Console.BufferHeight = value.Height;
				#endif
			}
		}

		public override Coordinates CursorPosition {
			get {
				return new Coordinates(
				#if !noConsole
					Console.CursorLeft, Console.CursorTop
				#else
					// 为 WinForms 返回一个虚拟值。
					0, 0
				#endif
				);
			}
			set {
				#if !noConsole
					Console.CursorTop = value.Y;
					Console.CursorLeft = value.X;
				#endif
			}
		}

		public override int CursorSize {
			get {
				return
					#if !noConsole
						Console.CursorSize
					#else
						// 为 WinForms 返回一个虚拟值。
						25
					#endif
				;
			}
			set {
				#if !noConsole
					Console.CursorSize = value;
				#endif
			}
		}

		#if noConsole
			private Form Invisible_Form;
		#endif

		public override void FlushInputBuffer() {
			#if !noConsole
			if (!Console_Info.IsInputRedirected()) {
				while (Console.KeyAvailable)
					Console.ReadKey(true);
			}
			#else
			if (Invisible_Form != null) {
				Invisible_Form.Close();
				Invisible_Form = null;
			} else {
				Invisible_Form = new Form();
				Invisible_Form.Opacity = 0;
				Invisible_Form.ShowInTaskbar = false;
				Invisible_Form.Visible = true;
			}
			#endif
		}

		public override ConsoleColor ForegroundColor {
			#if !noConsole
				get { return Console.ForegroundColor; }
				set { Console.ForegroundColor = value; }
			#else
				get { return _GUIForegroundColor; }
				set { _GUIForegroundColor = value; }
			#endif
		}

		public override BufferCell[, ] GetBufferContents(System.Management.Automation.Host.Rectangle rectangle) {
			#if !noConsole
			IntPtr hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
			CHAR_INFO[, ] buffer = new CHAR_INFO[rectangle.Bottom - rectangle.Top + 1, rectangle.Right - rectangle.Left + 1];
			COORD buffer_size = new COORD {
				X = (short)(rectangle.Right - rectangle.Left + 1), Y = (short)(rectangle.Bottom - rectangle.Top + 1)
			};
			COORD buffer_index = new COORD {
				X = 0, Y = 0
			};
			SMALL_RECT screen_rect = new SMALL_RECT {
				Left = (short) rectangle.Left, Top = (short) rectangle.Top, Right = (short) rectangle.Right, Bottom = (short) rectangle.Bottom
			};

			ReadConsoleOutput(hStdOut, buffer, buffer_size, buffer_index, ref screen_rect);

			System.Management.Automation.Host.BufferCell[, ] ScreenBuffer = new System.Management.Automation.Host.BufferCell[rectangle.Bottom - rectangle.Top + 1, rectangle.Right - rectangle.Left + 1];
			for (int y = 0; y <= rectangle.Bottom - rectangle.Top; y++)
				for (int x = 0; x <= rectangle.Right - rectangle.Left; x++) {
					ScreenBuffer[y, x] = new System.Management.Automation.Host.BufferCell(buffer[y, x].AsciiChar, (System.ConsoleColor)(buffer[y, x].Attributes & 0xF), (System.ConsoleColor)((buffer[y, x].Attributes & 0xF0) / 0x10), System.Management.Automation.Host.BufferCellType.Complete);
				}

			return ScreenBuffer;
			#else
			System.Management.Automation.Host.BufferCell[, ] ScreenBuffer = new System.Management.Automation.Host.BufferCell[rectangle.Bottom - rectangle.Top + 1, rectangle.Right - rectangle.Left + 1];

			for (int y = 0; y <= rectangle.Bottom - rectangle.Top; y++)
				for (int x = 0; x <= rectangle.Right - rectangle.Left; x++) {
					ScreenBuffer[y, x] = new System.Management.Automation.Host.BufferCell(' ', _GUIForegroundColor, _GUIBackgroundColor, System.Management.Automation.Host.BufferCellType.Complete);
				}

			return ScreenBuffer;
			#endif
		}

		public override bool KeyAvailable {
			get {
				return
					#if !noConsole
						Console.KeyAvailable
					#else
						true
					#endif
				;
			}
		}

		public override System.Management.Automation.Host.Size MaxPhysicalWindowSize {
			get {
				return new System.Management.Automation.Host.Size(
					#if !noConsole
						Console.LargestWindowWidth, Console.LargestWindowHeight
					#else
						// WinForms 的虚拟值
						240, 84
					#endif
				);
			}
		}

		public override System.Management.Automation.Host.Size MaxWindowSize {
			get {
				return new System.Management.Automation.Host.Size(
					#if !noConsole
						Console.BufferWidth, Console.BufferWidth
					#else
						// WinForms 的虚拟值
						120, 84
					#endif
				);
			}
		}

		public override KeyInfo ReadKey(ReadKeyOptions options) {
			#if !noConsole
			ConsoleKeyInfo info = Console.ReadKey((options & ReadKeyOptions.NoEcho) != 0);

			ControlKeyStates state = 0;
			if ((info.Modifiers & ConsoleModifiers.Alt) != 0)
				state |= ControlKeyStates.LeftAltPressed | ControlKeyStates.RightAltPressed;
			if ((info.Modifiers & ConsoleModifiers.Control) != 0)
				state |= ControlKeyStates.LeftCtrlPressed | ControlKeyStates.RightCtrlPressed;
			if ((info.Modifiers & ConsoleModifiers.Shift) != 0)
				state |= ControlKeyStates.ShiftPressed;
			if (Console.CapsLock)
				state |= ControlKeyStates.CapsLockOn;
			if (Console.NumberLock)
				state |= ControlKeyStates.NumLockOn;

			return new KeyInfo((int) info.Key, info.KeyChar, state, (options & ReadKeyOptions.IncludeKeyDown) != 0);
			#else
			if ((options & ReadKeyOptions.IncludeKeyDown) != 0)
				return ReadKey_Box.Show(_windowTitleData, "", true);
			else
				return ReadKey_Box.Show(_windowTitleData, "", false);
			#endif
		}

		public override void ScrollBufferContents(System.Management.Automation.Host.Rectangle source, Coordinates destination, System.Management.Automation.Host.Rectangle clip, BufferCell fill) { // 未实现目标块裁剪
			#if !noConsole
			// 裁剪区域超出源范围？
			if ((source.Left > clip.Right) || (source.Right < clip.Left) || (source.Top > clip.Bottom) || (source.Bottom < clip.Top)) { // 裁剪超出范围 -> 无需处理
				return;
			}

			IntPtr hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
			SMALL_RECT lpScrollRectangle = new SMALL_RECT {
				Left = (short) source.Left, Top = (short) source.Top, Right = (short)(source.Right), Bottom = (short)(source.Bottom)
			};
			SMALL_RECT lpClipRectangle;
			if (clip != null) {
				lpClipRectangle = new SMALL_RECT {
					Left = (short) clip.Left, Top = (short) clip.Top, Right = (short)(clip.Right), Bottom = (short)(clip.Bottom)
				};
			} else {
				lpClipRectangle = new SMALL_RECT {
					Left = (short) 0, Top = (short) 0, Right = (short)(Console.WindowWidth - 1), Bottom = (short)(Console.WindowHeight - 1)
				};
			}
			COORD dwDestinationOrigin = new COORD {
				X = (short)(destination.X), Y = (short)(destination.Y)
			};
			CHAR_INFO lpFill = new CHAR_INFO {
				AsciiChar = fill.Character, Attributes = (ushort)((int)(fill.ForegroundColor) + ((int)(fill.BackgroundColor) * 16))
			};

			ScrollConsoleScreenBuffer(hStdOut, ref lpScrollRectangle, ref lpClipRectangle, dwDestinationOrigin, ref lpFill);
			#endif
		}

		public override void SetBufferContents(System.Management.Automation.Host.Rectangle rectangle, BufferCell fill) {
			#if !noConsole
			// 用一个小技巧：把缓冲区移出屏幕，源区域便会被 fill.Character 字符填充
			if (rectangle.Left >= 0)
				Console.MoveBufferArea(rectangle.Left, rectangle.Top, rectangle.Right - rectangle.Left + 1, rectangle.Bottom - rectangle.Top + 1, BufferSize.Width, BufferSize.Height, fill.Character, fill.ForegroundColor, fill.BackgroundColor);
			else { // Clear-Host：把所有内容移出屏幕
				Console.MoveBufferArea(0, 0, BufferSize.Width, BufferSize.Height, BufferSize.Width, BufferSize.Height, fill.Character, fill.ForegroundColor, fill.BackgroundColor);
			}
			#endif
		}

		public override void SetBufferContents(Coordinates origin, BufferCell[, ] contents) {
			#if !noConsole
			IntPtr hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
			CHAR_INFO[, ] buffer = new CHAR_INFO[contents.GetLength(0), contents.GetLength(1)];
			COORD buffer_size = new COORD {
				X = (short)(contents.GetLength(1)), Y = (short)(contents.GetLength(0))
			};
			COORD buffer_index = new COORD {
				X = 0, Y = 0
			};
			SMALL_RECT screen_rect = new SMALL_RECT {
				Left = (short) origin.X, Top = (short) origin.Y, Right = (short)(origin.X + contents.GetLength(1) - 1), Bottom = (short)(origin.Y + contents.GetLength(0) - 1)
			};

			for (int y = 0; y < contents.GetLength(0); y++)
				for (int x = 0; x < contents.GetLength(1); x++) {
					buffer[y, x] = new CHAR_INFO {
						AsciiChar = contents[y, x].Character, Attributes = (ushort)((int)(contents[y, x].ForegroundColor) + ((int)(contents[y, x].BackgroundColor) * 16))
					};
				}

			WriteConsoleOutput(hStdOut, buffer, buffer_size, buffer_index, ref screen_rect);
			#endif
		}

		public override Coordinates WindowPosition {
			get {
				return new Coordinates(
					#if !noConsole
						Console.WindowLeft, Console.WindowTop
					#else
						// WinForms 的虚拟值
						0, 0
					#endif
				);
			}
			set {
				#if !noConsole
				Console.WindowLeft = value.X;
				Console.WindowTop = value.Y;
				#endif
			}
		}

		public override System.Management.Automation.Host.Size WindowSize {
			get {
				return new System.Management.Automation.Host.Size(
				#if !noConsole
					Console.WindowWidth, Console.WindowHeight
				#else
					// WinForms 的虚拟值
					120, 50
				#endif
				);
			}
			set {
				#if !noConsole
				Console.WindowWidth = value.Width;
				Console.WindowHeight = value.Height;
				#endif
			}
		}

		public override string WindowTitle {
			get {
				return
				#if !noConsole
					Console.Title
				#else
					_windowTitleData
				#endif
				;
			}
			set {
				#if !noConsole
					Console.Title
				#else
					_windowTitleData
				#endif
				= value;
			}
		}
	}

	#if noConsole
	public class Input_Box {
		[DllImport("user32.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
		private static extern IntPtr MB_GetString(uint strId);

		public static DialogResult Show(string strTitle, string strPrompt, ref string strVal, bool blSecure) {
			// 生成控件
			Form form = new Form();
			form.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			form.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			Label label = new Label();
			TextBox textBox = new TextBox();
			Button buttonOk = new Button();
			Button buttonCancel = new Button();

			// 尺寸和位置根据标签确定，必须先完成这个控件
			if (string.IsNullOrEmpty(strPrompt)) {
				if (blSecure)
					strPrompt = "Secure input:";
				else
					strPrompt = "Input:";
			}
			label.Text = strPrompt.PadRight(16);
			label.Location = new Point(9, 19);
			label.MaximumSize = new System.Drawing.Size(System.Windows.Forms.Screen.FromControl(form).Bounds.Width * 5 / 8 - 18, 0);
			label.AutoSize = true;
			// 标签的尺寸要到 Add() 之后才能确定
			form.Controls.Add(label);

			// 生成文本框
			if (blSecure) textBox.UseSystemPasswordChar = true;
			textBox.Text = strVal;
			textBox.SetBounds(12, label.Bottom, label.Right - 12, 20);

			// 生成按钮，并获取本地化的 "OK" 字符串
			string sTextOK = Marshal.PtrToStringUni(MB_GetString(0));
			if (string.IsNullOrEmpty(sTextOK))
				buttonOk.Text = "OK";
			else
				buttonOk.Text = sTextOK;

			// 获取本地化的 "Cancel" 字符串
			string sTextCancel = Marshal.PtrToStringUni(MB_GetString(1));
			if (string.IsNullOrEmpty(sTextCancel))
				buttonCancel.Text = "Cancel";
			else
				buttonCancel.Text = sTextCancel;

			buttonOk.DialogResult = DialogResult.OK;
			buttonCancel.DialogResult = DialogResult.Cancel;
			buttonOk.SetBounds(System.Math.Max(12, label.Right - 158), label.Bottom + 36, 75, 23);
			buttonCancel.SetBounds(System.Math.Max(93, label.Right - 77), label.Bottom + 36, 75, 23);

			// 配置窗体
			form.Text = strTitle;
			form.ClientSize = new System.Drawing.Size(System.Math.Max(178, label.Right + 10), label.Bottom + 71);
			form.Controls.AddRange(new Control[] {
				textBox,
				buttonOk,
				buttonCancel
			});
			form.FormBorderStyle = FormBorderStyle.FixedDialog;
			form.StartPosition = FormStartPosition.CenterScreen;
			try {
				form.Icon = Icon.ExtractAssociatedIcon(Assembly.GetEntryAssembly().Location);
			} catch {}
			form.MinimizeBox = false;
			form.MaximizeBox = false;
			form.AcceptButton = buttonOk;
			form.CancelButton = buttonCancel;

			// 显示窗体并计算结果
			DialogResult dialogResult = form.ShowDialog();
			strVal = textBox.Text;
			return dialogResult;
		}

		public static DialogResult Show(string strTitle, string strPrompt, ref string strVal) {
			return Show(strTitle, strPrompt, ref strVal, false);
		}
	}

	public class Choice_Box {
		public static int Show(System.Collections.ObjectModel.Collection<ChoiceDescription> arrChoice, int intDefault, string strTitle, string strPrompt) {
			// 数组为空则取消
			if (arrChoice == null) return -1;
			if (arrChoice.Count < 1) return -1;

			// 生成控件
			Form form = new Form();
			form.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			form.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			RadioButton[] aradioButton = new RadioButton[arrChoice.Count];
			ToolTip toolTip = new ToolTip();
			Button buttonOk = new Button();

			// 尺寸和位置根据标签确定，有提示时必须先完成这个控件
			int iPosY = 19, iMaxX = 0;
			if (!string.IsNullOrEmpty(strPrompt)) {
				Label label = new Label();
				label.Text = strPrompt;
				label.Location = new Point(9, 19);
				label.MaximumSize = new System.Drawing.Size(System.Windows.Forms.Screen.FromControl(form).Bounds.Width * 5 / 8 - 18, 0);
				label.AutoSize = true;
				// 标签的尺寸要到 Add() 之后才能确定
				form.Controls.Add(label);
				iPosY = label.Bottom;
				iMaxX = label.Right;
			}

			// 其余尺寸和位置以单选按钮为基准，因此现在就完成这些控件
			int Counter = 0;
			int tempWidth = System.Windows.Forms.Screen.FromControl(form).Bounds.Width * 5 / 8 - 18;
			foreach(ChoiceDescription sAuswahl in arrChoice) {
				aradioButton[Counter] = new RadioButton();
				aradioButton[Counter].Text = Regex.Replace(sAuswahl.Label, ".\b", "");
				if (Counter == intDefault)
					aradioButton[Counter].Checked = true;
				aradioButton[Counter].Location = new Point(9, iPosY);
				aradioButton[Counter].AutoSize = true;
				// 标签的尺寸要到 Add() 之后才能确定
				form.Controls.Add(aradioButton[Counter]);
				if (aradioButton[Counter].Width > tempWidth) { // 单选按钮对屏幕来说太宽 -> 换成两行
					int tempHeight = aradioButton[Counter].Height;
					aradioButton[Counter].Height = tempHeight * (1 + (aradioButton[Counter].Width - 1) / tempWidth);
					aradioButton[Counter].Width = tempWidth;
					aradioButton[Counter].AutoSize = false;
				}
				iPosY = aradioButton[Counter].Bottom;
				if (aradioButton[Counter].Right > iMaxX) {
					iMaxX = aradioButton[Counter].Right;
				}
				if (!string.IsNullOrEmpty(sAuswahl.HelpMessage))
					toolTip.SetToolTip(aradioButton[Counter], sAuswahl.HelpMessage);
				Counter++;
			}

			// 父窗口不活动时也显示工具提示
			toolTip.ShowAlways = true;

			// 创建按钮
			buttonOk.Text = "OK";
			buttonOk.DialogResult = DialogResult.OK;
			buttonOk.SetBounds(System.Math.Max(12, iMaxX - 77), iPosY + 36, 75, 23);

			// 配置窗体
			form.Text = strTitle;
			form.ClientSize = new System.Drawing.Size(System.Math.Max(178, iMaxX + 10), iPosY + 71);
			form.Controls.Add(buttonOk);
			form.FormBorderStyle = FormBorderStyle.FixedDialog;
			form.StartPosition = FormStartPosition.CenterScreen;
			try {
				form.Icon = Icon.ExtractAssociatedIcon(Assembly.GetEntryAssembly().Location);
			} catch {}
			form.MinimizeBox = false;
			form.MaximizeBox = false;
			form.AcceptButton = buttonOk;

			// 显示并计算窗体
			if (form.ShowDialog() != DialogResult.OK)
				return -1;
			int iRueck = -1;
			for (Counter = 0; Counter < arrChoice.Count; Counter++) {
				if (aradioButton[Counter].Checked == true) {
					iRueck = Counter;
				}
			}
			return iRueck;
		}
	}

	public class ReadKey_Box {
		[DllImport("user32.dll")]
		public static extern int ToUnicode(uint wVirtKey, uint wScanCode, byte[] lpKeyState,
			[Out, MarshalAs(UnmanagedType.LPWStr, SizeConst = 64)] System.Text.StringBuilder pwszBuff,
			int cchBuff, uint wFlags);

		static string GetCharFromKeys(Keys keys, bool blShift, bool blAltGr) {
			System.Text.StringBuilder buffer = new System.Text.StringBuilder(64);
			byte[] keyboardState = new byte[256];
			if (blShift)
				keyboardState[(int) Keys.ShiftKey] = 0xff;
			if (blAltGr) {
				keyboardState[(int) Keys.ControlKey] = 0xff;
				keyboardState[(int) Keys.Menu] = 0xff;
			}
			if (ToUnicode((uint) keys, 0, keyboardState, buffer, 64, 0) >= 1)
				return buffer.ToString();
			else
				return "\0";
		}

		class Keyboard_Form: Form {
			public Keyboard_Form() {
				this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
				this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
				this.KeyDown += new KeyEventHandler(Keyboard_Form_KeyDown);
				this.KeyUp += new KeyEventHandler(Keyboard_Form_KeyUp);
			}

			// 检查 KeyDown 还是 KeyUp？
			public bool checkKeyDown = true;
			// 按键的键码
			public KeyInfo keyinfo;

			void Keyboard_Form_KeyDown(object sender, KeyEventArgs kevent) {
				if (checkKeyDown) { // 存储按键信息
					keyinfo.VirtualKeyCode = kevent.KeyValue;
					keyinfo.Character = GetCharFromKeys(kevent.KeyCode, kevent.Shift, kevent.Alt & kevent.Control)[0];
					keyinfo.KeyDown = false;
					keyinfo.ControlKeyState = 0;
					if (kevent.Alt) {
						keyinfo.ControlKeyState = ControlKeyStates.LeftAltPressed | ControlKeyStates.RightAltPressed;
					}
					if (kevent.Control) {
						keyinfo.ControlKeyState |= ControlKeyStates.LeftCtrlPressed | ControlKeyStates.RightCtrlPressed;
						if (!kevent.Alt)
							if (kevent.KeyValue > 64 && kevent.KeyValue < 96)
								keyinfo.Character = (char)(kevent.KeyValue - 64);
					}
					if (kevent.Shift) {
						keyinfo.ControlKeyState |= ControlKeyStates.ShiftPressed;
					}
					if ((kevent.Modifiers & System.Windows.Forms.Keys.CapsLock) > 0) {
						keyinfo.ControlKeyState |= ControlKeyStates.CapsLockOn;
					}
					if ((kevent.Modifiers & System.Windows.Forms.Keys.NumLock) > 0) {
						keyinfo.ControlKeyState |= ControlKeyStates.NumLockOn;
					}
					// 然后关闭窗体
					this.Close();
				}
			}

			void Keyboard_Form_KeyUp(object sender, KeyEventArgs kevent) {
				if (!checkKeyDown) { // 存储按键信息
					keyinfo.VirtualKeyCode = kevent.KeyValue;
					keyinfo.Character = GetCharFromKeys(kevent.KeyCode, kevent.Shift, kevent.Alt & kevent.Control)[0];
					keyinfo.KeyDown = true;
					keyinfo.ControlKeyState = 0;
					if (kevent.Alt) {
						keyinfo.ControlKeyState = ControlKeyStates.LeftAltPressed | ControlKeyStates.RightAltPressed;
					}
					if (kevent.Control) {
						keyinfo.ControlKeyState |= ControlKeyStates.LeftCtrlPressed | ControlKeyStates.RightCtrlPressed;
						if (!kevent.Alt)
							if (kevent.KeyValue > 64 && kevent.KeyValue < 96)
								keyinfo.Character = (char)(kevent.KeyValue - 64);
					}
					if (kevent.Shift) {
						keyinfo.ControlKeyState |= ControlKeyStates.ShiftPressed;
					}
					if ((kevent.Modifiers & System.Windows.Forms.Keys.CapsLock) > 0) {
						keyinfo.ControlKeyState |= ControlKeyStates.CapsLockOn;
					}
					if ((kevent.Modifiers & System.Windows.Forms.Keys.NumLock) > 0) {
						keyinfo.ControlKeyState |= ControlKeyStates.NumLockOn;
					}
					// 然后关闭窗体
					this.Close();
				}
			}
		}

		public static KeyInfo Show(string strTitle, string strPrompt, bool blIncludeKeyDown) {
			// 创建控件
			Keyboard_Form form = new Keyboard_Form();
			Label label = new Label();

			// 尺寸和位置以标签为基准，因此先完成这个控件
			if (string.IsNullOrEmpty(strPrompt))
				label.Text = "Press a key";
			else
				label.Text = strPrompt;
			label.Location = new Point(9, 19);
			label.MaximumSize = new System.Drawing.Size(System.Windows.Forms.Screen.FromControl(form).Bounds.Width * 5 / 8 - 18, 0);
			label.AutoSize = true;
			// 标签的尺寸要到 Add() 之后才能确定
			form.Controls.Add(label);

			// 配置窗体
			form.Text = strTitle;
			form.ClientSize = new System.Drawing.Size(System.Math.Max(178, label.Right + 10), label.Bottom + 55);
			form.FormBorderStyle = FormBorderStyle.FixedDialog;
			form.StartPosition = FormStartPosition.CenterScreen;
			try {
				form.Icon = Icon.ExtractAssociatedIcon(Assembly.GetEntryAssembly().Location);
			} catch {}
			form.MinimizeBox = false;
			form.MaximizeBox = false;

			// 显示并计算窗体
			form.checkKeyDown = blIncludeKeyDown;
			form.ShowDialog();
			return form.keyinfo;
		}
	}

	public class Progress_Form: Form {
		private ConsoleColor ProgressBarColor = ConsoleColor.DarkCyan;
		private string WindowTitle = "";

		#if !noVisualStyles
		private System.Timers.Timer _timer = new System.Timers.Timer();
		private int _barNumber = -1;
		private int _barValue = -1;
		private bool _inTick = false;
		#endif

		struct Progress_Data {
			internal Label lbActivity;
			internal Label lbStatus;
			internal ProgressBar objProgressBar;
			internal Label lbRemainingTime;
			internal Label lbOperation;
			internal int ActivityId;
			internal int ParentActivityId;
			internal int Depth;
		};

		private List<Progress_Data> progressDataList = new List<Progress_Data> ();

		public Progress_Form(string Title, ConsoleColor BarColor) {
			WindowTitle = Title;
			ProgressBarColor = BarColor;
			InitializeComponent();
		}
		private void InitializeComponent() {
			this.SuspendLayout();

			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;

			this.AutoScroll = true;
			this.Text = WindowTitle;
			this.Height = 147;
			this.Width = 800;
			this.BackColor = Color.White;
			this.FormBorderStyle = FormBorderStyle.FixedSingle;
			this.MinimizeBox = false;
			this.MaximizeBox = false;
			this.ControlBox = false;
			this.StartPosition = FormStartPosition.CenterScreen;

			this.ResumeLayout();
			#if !noVisualStyles
			_timer.Elapsed += new System.Timers.ElapsedEventHandler(TimeTick);
			_timer.Interval = 50; // 毫秒
			_timer.AutoReset = true;
			_timer.Start();
			#endif
		}

		private Color DrawingColor(ConsoleColor color) { // 把 ConsoleColor 转换为 System.Drawing.Color
			switch (color) {
			case ConsoleColor.DarkYellow:
				return ColorTranslator.FromOle(35723);//#8B8B00
			default:
				return Color.FromName(color.ToString());
			}
		}

		#if !noVisualStyles
		private void TimeTick(object source, System.Timers.ElapsedEventArgs eventargs) { // 由 _timer 事件调用的工作函数
			if (_inTick) return;
			_inTick = true;
			if (_barNumber >= 0) {
				if (_barValue >= 0) {
					progressDataList[_barNumber].objProgressBar.Value = _barValue;
					_barValue = -1;
				}
				progressDataList[_barNumber].objProgressBar.Refresh();
			}
			_inTick = false;
		}
		#endif

		private void AddBar(ref Progress_Data pd, int position) {
			// 创建标签
			pd.lbActivity = new Label();
			pd.lbActivity.Left = 5;
			pd.lbActivity.Top = 104 * position + 10;
			pd.lbActivity.Width = 800 - 20;
			pd.lbActivity.Height = 16;
			pd.lbActivity.Font = new Font(pd.lbActivity.Font, FontStyle.Bold);
			pd.lbActivity.Text = "";
			// 把标签添加到窗体
			this.Controls.Add(pd.lbActivity);

			// 创建标签
			pd.lbStatus = new Label();
			pd.lbStatus.Left = 25;
			pd.lbStatus.Top = 104 * position + 26;
			pd.lbStatus.Width = 800 - 40;
			pd.lbStatus.Height = 16;
			pd.lbStatus.Text = "";
			// 把标签添加到窗体
			this.Controls.Add(pd.lbStatus);

			// 创建进度条
			pd.objProgressBar = new ProgressBar();
			pd.objProgressBar.Value = 0;
			pd.objProgressBar.Style =
				#if noVisualStyles
					ProgressBarStyle.Continuous
				#else
					ProgressBarStyle.Blocks
				#endif
			;
			pd.objProgressBar.ForeColor = DrawingColor(ProgressBarColor);
			if (pd.Depth < 15) {
				pd.objProgressBar.Size = new System.Drawing.Size(800 - 60 - 30 * pd.Depth, 20);
				pd.objProgressBar.Left = 25 + 30 * pd.Depth;
			} else {
				pd.objProgressBar.Size = new System.Drawing.Size(800 - 60 - 450, 20);
				pd.objProgressBar.Left = 25 + 450;
			}
			pd.objProgressBar.Top = 104 * position + 47;
			// 把进度条添加到窗体
			this.Controls.Add(pd.objProgressBar);

			// 创建标签
			pd.lbRemainingTime = new Label();
			pd.lbRemainingTime.Left = 5;
			pd.lbRemainingTime.Top = 104 * position + 72;
			pd.lbRemainingTime.Width = 800 - 20;
			pd.lbRemainingTime.Height = 16;
			pd.lbRemainingTime.Text = "";
			// 把标签添加到窗体
			this.Controls.Add(pd.lbRemainingTime);

			// 创建标签
			pd.lbOperation = new Label();
			pd.lbOperation.Left = 25;
			pd.lbOperation.Top = 104 * position + 88;
			pd.lbOperation.Width = 800 - 40;
			pd.lbOperation.Height = 16;
			pd.lbOperation.Text = "";
			// 把标签添加到窗体
			this.Controls.Add(pd.lbOperation);
		}

		public int GetCount() {
			return progressDataList.Count;
		}

		public void Update(ProgressRecord objRecord) {
			if (objRecord == null)
				return;

			int currentProgress = -1;
			for (int i = 0; i < progressDataList.Count; i++) {
				if (progressDataList[i].ActivityId == objRecord.ActivityId) {
					currentProgress = i;
					break;
				}
			}

			if (objRecord.RecordType == ProgressRecordType.Completed) {
				if (currentProgress >= 0) {
					#if !noVisualStyles
					if (_barNumber == currentProgress) _barNumber = -1;
					#endif
					this.Controls.Remove(progressDataList[currentProgress].lbActivity);
					this.Controls.Remove(progressDataList[currentProgress].lbStatus);
					this.Controls.Remove(progressDataList[currentProgress].objProgressBar);
					this.Controls.Remove(progressDataList[currentProgress].lbRemainingTime);
					this.Controls.Remove(progressDataList[currentProgress].lbOperation);

					progressDataList[currentProgress].lbActivity.Dispose();
					progressDataList[currentProgress].lbStatus.Dispose();
					progressDataList[currentProgress].objProgressBar.Dispose();
					progressDataList[currentProgress].lbRemainingTime.Dispose();
					progressDataList[currentProgress].lbOperation.Dispose();

					progressDataList.RemoveAt(currentProgress);
				}

				if (progressDataList.Count == 0) {
					#if !noVisualStyles
					_timer.Stop();
					_timer.Dispose();
					#endif
					this.Close();
					return;
				}

				if (currentProgress < 0) return;

				for (int i = currentProgress; i < progressDataList.Count; i++) {
					progressDataList[i].lbActivity.Top = 104 * i + 10;
					progressDataList[i].lbStatus.Top = 104 * i + 26;
					progressDataList[i].objProgressBar.Top = 104 * i + 47;
					progressDataList[i].lbRemainingTime.Top = 104 * i + 72;
					progressDataList[i].lbOperation.Top = 104 * i + 88;
				}

				if (104 * progressDataList.Count + 43 <= System.Windows.Forms.Screen.FromControl(this).Bounds.Height) {
					this.Height = 104 * progressDataList.Count + 43;
					this.Location = new Point((System.Windows.Forms.Screen.FromControl(this).Bounds.Width - this.Width) / 2, (System.Windows.Forms.Screen.FromControl(this).Bounds.Height - this.Height) / 2);
				} else {
					this.Height = System.Windows.Forms.Screen.FromControl(this).Bounds.Height;
					this.Location = new Point((System.Windows.Forms.Screen.FromControl(this).Bounds.Width - this.Width) / 2, 0);
				}

				return;
			}

			if (currentProgress < 0) {
				Progress_Data pd = new Progress_Data();
				pd.ActivityId = objRecord.ActivityId;
				pd.ParentActivityId = objRecord.ParentActivityId;
				pd.Depth = 0;

				int nextid = -1;
				int parentid = -1;
				if (pd.ParentActivityId >= 0) {
					for (int i = 0; i < progressDataList.Count; i++) {
						if (progressDataList[i].ActivityId == pd.ParentActivityId) {
							parentid = i;
							break;
						}
					}
				}

				if (parentid >= 0) {
					pd.Depth = progressDataList[parentid].Depth + 1;

					for (int i = parentid + 1; i < progressDataList.Count; i++) {
						if ((progressDataList[i].Depth < pd.Depth) || ((progressDataList[i].Depth == pd.Depth) && (progressDataList[i].ParentActivityId != pd.ParentActivityId))) {
							nextid = i;
							break;
						}
					}
				}

				if (nextid == -1) {
					AddBar(ref pd, progressDataList.Count);
					currentProgress = progressDataList.Count;
					progressDataList.Add(pd);
				} else {
					AddBar(ref pd, nextid);
					currentProgress = nextid;
					progressDataList.Insert(nextid, pd);

					for (int i = currentProgress + 1; i < progressDataList.Count; i++) {
						progressDataList[i].lbActivity.Top = 104 * i + 10;
						progressDataList[i].lbStatus.Top = 104 * i + 26;
						progressDataList[i].objProgressBar.Top = 104 * i + 47;
						progressDataList[i].lbRemainingTime.Top = 104 * i + 72;
						progressDataList[i].lbOperation.Top = 104 * i + 88;
					}
				}
				if (104 * progressDataList.Count + 43 <= System.Windows.Forms.Screen.FromControl(this).Bounds.Height) {
					this.Height = 104 * progressDataList.Count + 43;
					this.Location = new Point((System.Windows.Forms.Screen.FromControl(this).Bounds.Width - this.Width) / 2, (System.Windows.Forms.Screen.FromControl(this).Bounds.Height - this.Height) / 2);
				} else {
					this.Height = System.Windows.Forms.Screen.FromControl(this).Bounds.Height;
					this.Location = new Point((System.Windows.Forms.Screen.FromControl(this).Bounds.Width - this.Width) / 2, 0);
				}
			}

			if (!string.IsNullOrEmpty(objRecord.Activity))
				progressDataList[currentProgress].lbActivity.Text = objRecord.Activity;
			else
				progressDataList[currentProgress].lbActivity.Text = "";

			if (!string.IsNullOrEmpty(objRecord.StatusDescription))
				progressDataList[currentProgress].lbStatus.Text = objRecord.StatusDescription;
			else
				progressDataList[currentProgress].lbStatus.Text = "";

			if ((objRecord.PercentComplete >= 0) && (objRecord.PercentComplete <= 100)) {
				#if !noVisualStyles
				if (objRecord.PercentComplete < 100)
					progressDataList[currentProgress].objProgressBar.Value = objRecord.PercentComplete + 1;
				else
					progressDataList[currentProgress].objProgressBar.Value = 99;
				progressDataList[currentProgress].objProgressBar.Visible = true;
				_barNumber = currentProgress;
				_barValue = objRecord.PercentComplete;
				#else
				progressDataList[currentProgress].objProgressBar.Value = objRecord.PercentComplete;
				progressDataList[currentProgress].objProgressBar.Visible = true;
				#endif
			} else {
				if (objRecord.PercentComplete > 100) {
					progressDataList[currentProgress].objProgressBar.Value = 0;
					progressDataList[currentProgress].objProgressBar.Visible = true;
					#if !noVisualStyles
					_barNumber = currentProgress;
					_barValue = 0;
					#endif
				} else {
					progressDataList[currentProgress].objProgressBar.Visible = false;
					#if !noVisualStyles
					if (_barNumber == currentProgress) _barNumber = -1;
					#endif
				}
			}

			if (objRecord.SecondsRemaining >= 0) {
				System.TimeSpan objTimeSpan = new System.TimeSpan(0, 0, objRecord.SecondsRemaining);
				progressDataList[currentProgress].lbRemainingTime.Text = string.Format("Remaining time: {0:00}:{1:00}:{2:00}", (int) objTimeSpan.TotalHours, objTimeSpan.Minutes, objTimeSpan.Seconds);
			} else
				progressDataList[currentProgress].lbRemainingTime.Text = "";

			if (!string.IsNullOrEmpty(objRecord.CurrentOperation))
				progressDataList[currentProgress].lbOperation.Text = objRecord.CurrentOperation;
			else
				progressDataList[currentProgress].lbOperation.Text = "";

			Application.DoEvents();
		}
	}
	#endif

	// 在这里定义 IsInputRedirected()、IsOutputRedirected() 和 IsErrorRedirected()，因为它们最早是在 .NET 4.5 中引入的
	public class Console_Info {
		private enum FileType: uint {
			FILE_TYPE_UNKNOWN = 0x0000,
			FILE_TYPE_DISK = 0x0001,
			FILE_TYPE_CHAR = 0x0002,
			FILE_TYPE_PIPE = 0x0003,
			FILE_TYPE_REMOTE = 0x8000
		}

		private enum STDHandle: uint {
			STD_INPUT_HANDLE = unchecked((uint) - 10),
			STD_OUTPUT_HANDLE = unchecked((uint) - 11),
			STD_ERROR_HANDLE = unchecked((uint) - 12)
		}
		private enum ConsoleMode: uint {
			ENABLE_ECHO_INPUT = 0x0004,
			ENABLE_INSERT_MODE = 0x0020,
			ENABLE_LINE_INPUT = 0x0002,
			ENABLE_MOUSE_INPUT = 0x0010,
			ENABLE_PROCESSED_INPUT = 0x0001,
			ENABLE_QUICK_EDIT_MODE = 0x0040,
			ENABLE_WINDOW_INPUT = 0x0008,
			ENABLE_VIRTUAL_TERMINAL_INPUT = 0x0200,

			ENABLE_PROCESSED_OUTPUT = 0x0001,
			ENABLE_WRAP_AT_EOL_OUTPUT = 0x0002,
			ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004,
			DISABLE_NEWLINE_AUTO_RETURN = 0x0008,
			ENABLE_LVB_GRID_WORLDWIDE = 0x0010
		}

		[DllImport("Kernel32.dll")]
		static private extern UIntPtr GetStdHandle(STDHandle stdHandle);

		[DllImport("Kernel32.dll")]
		static private extern FileType GetFileType(UIntPtr hFile);
		[DllImport("Kernel32.dll")]
		static private extern bool GetConsoleMode(UIntPtr hConsoleHandle, out ConsoleMode lpConsoleMode);
		[DllImport("Kernel32.dll")]
		static private extern bool SetConsoleMode(UIntPtr hConsoleHandle, ConsoleMode dwMode);

		static public bool IsInputRedirected() {
			UIntPtr hInput = GetStdHandle(STDHandle.STD_INPUT_HANDLE);
			FileType fileType = GetFileType(hInput);
			if ((fileType == FileType.FILE_TYPE_CHAR) || (fileType == FileType.FILE_TYPE_UNKNOWN))
				return false;
			return true;
		}

		static public bool IsOutputRedirected() {
			UIntPtr hOutput = GetStdHandle(STDHandle.STD_OUTPUT_HANDLE);
			FileType fileType = GetFileType(hOutput);
			if ((fileType == FileType.FILE_TYPE_CHAR) || (fileType == FileType.FILE_TYPE_UNKNOWN))
				return false;
			return true;
		}

		static public bool IsErrorRedirected() {
			UIntPtr hError = GetStdHandle(STDHandle.STD_ERROR_HANDLE);
			FileType fileType = GetFileType(hError);
			if ((fileType == FileType.FILE_TYPE_CHAR) || (fileType == FileType.FILE_TYPE_UNKNOWN))
				return false;
			return true;
		}
		static public bool IsVirtualTerminalSupported() {
			UIntPtr hOutput = GetStdHandle(STDHandle.STD_OUTPUT_HANDLE);
			ConsoleMode consoleMode;
			if(!GetConsoleMode(hOutput, out consoleMode))
				return false;
			return (consoleMode & ConsoleMode.ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
		}
	}

	internal class PSRunnerUI: PSHostUserInterface {
		public PSRunnerRawUI rawUI;

		public ConsoleColor ErrorForegroundColor = ConsoleColor.Red;
		public ConsoleColor ErrorBackgroundColor = ConsoleColor.Black;

		public ConsoleColor WarningForegroundColor = ConsoleColor.Yellow;
		public ConsoleColor WarningBackgroundColor = ConsoleColor.Black;

		public ConsoleColor DebugForegroundColor = ConsoleColor.Yellow;
		public ConsoleColor DebugBackgroundColor = ConsoleColor.Black;

		public ConsoleColor VerboseForegroundColor = ConsoleColor.Yellow;
		public ConsoleColor VerboseBackgroundColor = ConsoleColor.Black;

		public ConsoleColor ProgressForegroundColor =
		#if !noConsole
			ConsoleColor.Yellow
		#else
			ConsoleColor.DarkCyan
		#endif
		;
		public ConsoleColor ProgressBackgroundColor = ConsoleColor.DarkCyan;

		public PSRunnerUI() {
			rawUI = new PSRunnerRawUI();
			#if !noConsole
				rawUI.ForegroundColor = Console.ForegroundColor;
				rawUI.BackgroundColor = Console.BackgroundColor;
			#endif
		}

		#if !Pwsh20
			public override bool SupportsVirtualTerminal { get { return Console_Info.IsVirtualTerminalSupported(); } }
		#endif

		public override Dictionary<string, PSObject> Prompt(string caption, string message, System.Collections.ObjectModel.Collection<FieldDescription> descriptions) {
			#if !noConsole
				if (!string.IsNullOrEmpty(caption)) WriteLine(caption);
				if (!string.IsNullOrEmpty(message)) WriteLine(message);
			#else
				if ((!string.IsNullOrEmpty(caption)) || (!string.IsNullOrEmpty(message))) {
					string sTitle = rawUI.WindowTitle, sMeldung = "";

					if (!string.IsNullOrEmpty(caption)) sTitle = caption;
					if (!string.IsNullOrEmpty(message)) sMeldung = message;
					MessageBox.Show(sMeldung, sTitle);
				}

				// 重置 Input_Box 的标签文本
				_ib_message = "";
			#endif
			Dictionary<string, PSObject> ret = new Dictionary<string, PSObject> ();
			foreach(FieldDescription cd in descriptions) {
				Type type;
				if (string.IsNullOrEmpty(cd.ParameterAssemblyFullName))
					type = typeof(string);
				else
					type = Type.GetType(cd.ParameterAssemblyFullName);

				if (type.IsArray) {
					Type elementType = type.GetElementType();
					Type genericListType = Type.GetType("System.Collections.Generic.List\x60\x31");
					genericListType = genericListType.MakeGenericType(new [] {
						elementType
					});
					ConstructorInfo constructor = genericListType.GetConstructor(BindingFlags.CreateInstance | BindingFlags.Instance | BindingFlags.Public, null, Type.EmptyTypes, null);
					object resultList = constructor.Invoke(null);

					int index = 0;
					string data;
					do {
						if (!string.IsNullOrEmpty(cd.Name))
							#if !noConsole
								Write(string.Format("{0}[{1}]: ", cd.Name, index));
							#else
								_ib_message = string.Format("{0}[{1}]: ", cd.Name, index);
							#endif
						data = ReadLine();
						if (string.IsNullOrEmpty(data))
							break;
						object obj = System.Convert.ChangeType(data, elementType);
						genericListType.InvokeMember("Add", BindingFlags.InvokeMethod | BindingFlags.Public | BindingFlags.Instance, null, resultList, new [] {
							obj
						});
						index++;
					} while (true);

					System.Array retArray = (System.Array) genericListType.InvokeMember("ToArray", BindingFlags.InvokeMethod | BindingFlags.Public | BindingFlags.Instance, null, resultList, null);
					ret.Add(cd.Name, new PSObject(retArray));
				} else {
					object obj=null;
					string line;
					if (type != typeof(System.Security.SecureString)) {
						if (type != typeof(System.Management.Automation.PSCredential)) {
							#if !noConsole
							if (!string.IsNullOrEmpty(cd.Name)) Write(cd.Name);
							if (!string.IsNullOrEmpty(cd.HelpMessage)) Write(" (Type !? for help.)");
							if ((!string.IsNullOrEmpty(cd.Name)) || (!string.IsNullOrEmpty(cd.HelpMessage))) Write(": ");
							#else
							if (!string.IsNullOrEmpty(cd.Name)) _ib_message = string.Format("{0}: ", cd.Name);
							if (!string.IsNullOrEmpty(cd.HelpMessage)) _ib_message += "\n(Type !? for help.)";
							#endif
							do {
								line = ReadLine();
								if (line == "!?")
									WriteLine(cd.HelpMessage);
								else {
									if (string.IsNullOrEmpty(line)) obj = cd.DefaultValue;
									if (obj == null) {
										try {
											obj = System.Convert.ChangeType(line, type);
										} catch {
											Write("Wrong format, please repeat input: ");
											line = "!?";
										}
									}
								}
							} while (line == "!?");
						} else
							obj = PromptForCredential("", "", "", "");
					} else {
						if (!string.IsNullOrEmpty(cd.Name))
							#if !noConsole
								Write(string.Format("{0}: ", cd.Name));
							#else
								_ib_message = string.Format("{0}: ", cd.Name);
							#endif

						obj = ReadLineAsSecureString();
					}

					ret.Add(cd.Name, new PSObject(obj));
				}
			}
			#if noConsole
			// 重置 Input_Box 的标签文本
			_ib_message = "";
			#endif
			return ret;
		}

		public override int PromptForChoice(string caption, string message, System.Collections.ObjectModel.Collection<ChoiceDescription> choices, int defaultChoice) {
			#if noConsole
			if (string.IsNullOrEmpty(caption)) caption = rawUI.WindowTitle;
			int iReturn = Choice_Box.Show(choices, defaultChoice, caption, message);
			if (iReturn == -1)
				iReturn = defaultChoice;
			return iReturn;
			#else
			if (!string.IsNullOrEmpty(caption)) WriteLine(caption);
			WriteLine(message);
			do {
				int idx = 0;
				SortedList<string, int> res = new SortedList<string, int> ();
				string defkey = "";
				foreach(ChoiceDescription cd in choices) {
					string lkey = cd.Label.Substring(0, 1), ltext = cd.Label;
					int pos = cd.Label.IndexOf('&');
					if (pos > -1) {
						lkey = cd.Label.Substring(pos + 1, 1).ToUpper();
						if (pos > 0)
							ltext = cd.Label.Substring(0, pos) + cd.Label.Substring(pos + 1);
						else
							ltext = cd.Label.Substring(1);
					}
					res.Add(lkey.ToLower(), idx);

					if (idx > 0) Write("  ");
					ConsoleColor fg = rawUI.ForegroundColor, bg = rawUI.BackgroundColor;
					if (idx == defaultChoice) {
						fg = VerboseForegroundColor;
						defkey = lkey;
					}
					Write(fg, bg, string.Format("[{0}] {1}", lkey, ltext));
					idx++;
				}
				Write(rawUI.ForegroundColor, rawUI.BackgroundColor, string.Format("  [?] Help (default is \"{0}\"): ", defkey));

				string inpkey = "";
				try {
					inpkey = Console.ReadLine().ToLower();
					if (res.ContainsKey(inpkey)) return res[inpkey];
					if (string.IsNullOrEmpty(inpkey)) return defaultChoice;
				} catch {/* 忽略部分读取错误 */}
				if (inpkey == "?") {
					foreach(ChoiceDescription cd in choices) {
						string lkey = cd.Label.Substring(0, 1);
						int pos = cd.Label.IndexOf('&');
						if (pos > -1) lkey = cd.Label.Substring(pos + 1, 1).ToUpper();
						if (!string.IsNullOrEmpty(cd.HelpMessage))
							WriteLine(rawUI.ForegroundColor, rawUI.BackgroundColor, string.Format("{0} - {1}", lkey, cd.HelpMessage));
						else
							WriteLine(rawUI.ForegroundColor, rawUI.BackgroundColor, string.Format("{0} -", lkey));
					}
				}
			} while (true);
			#endif
		}

		public override PSCredential PromptForCredential(string caption, string message, string userName, string targetName, PSCredentialTypes allowedCredentialTypes, PSCredentialUIOptions options) {
			#if !(noConsole || credentialGUI)
			if (!string.IsNullOrEmpty(caption)) WriteLine(caption);
			WriteLine(message);

			string UserName;
			Write("User name: ");
			if ((string.IsNullOrEmpty(userName)) || ((options & PSCredentialUIOptions.ReadOnlyUserName) == 0))
				UserName = ReadLine();
			else {
				if (!string.IsNullOrEmpty(targetName)) Write(targetName + "\\");
				WriteLine(userName);
				UserName = userName;
			}
			Write("Password: ");
			SecureString password = ReadLineAsSecureString();

			if (string.IsNullOrEmpty(UserName)) UserName = "<NOUSER>";
			if (!string.IsNullOrEmpty(targetName))
				if (UserName.IndexOf('\\') < 0)
					UserName = targetName + "\\" + UserName;

			return new PSCredential(UserName, password);
			#else
			Credential_Form.User_Pwd cred = Credential_Form.PromptForPassword(caption, message, targetName, userName, allowedCredentialTypes, options);
			if (cred != null) {
				System.Security.SecureString x = new System.Security.SecureString();
				foreach(char c in cred.Password.ToCharArray())
					x.AppendChar(c);

				return new PSCredential(cred.User, x);
			}
			return null;
			#endif
		}

		public override PSCredential PromptForCredential(string caption, string message, string userName, string targetName) {
			#if !(noConsole || credentialGUI)
				if (!string.IsNullOrEmpty(caption)) WriteLine(caption);
				WriteLine(message);

				string un;
				Write("User name: ");
				if (string.IsNullOrEmpty(userName))
					un = ReadLine();
				else {
					if (!string.IsNullOrEmpty(targetName)) Write(targetName + "\\");
					WriteLine(userName);
					un = userName;
				}
				SecureString pwd;
				Write("Password: ");
				pwd = ReadLineAsSecureString();

				if (string.IsNullOrEmpty(un)) un = "<NOUSER>";
				if (!string.IsNullOrEmpty(targetName)) {
					if (un.IndexOf('\\') < 0)
						un = targetName + "\\" + un;
				}

				PSCredential c2 = new PSCredential(un, pwd);
				return c2;
			#else
				Credential_Form.User_Pwd cred = Credential_Form.PromptForPassword(caption, message, targetName, userName, PSCredentialTypes.Default, PSCredentialUIOptions.Default);
				if (cred != null) {
					System.Security.SecureString x = new System.Security.SecureString();
					foreach(char c in cred.Password.ToCharArray())
					x.AppendChar(c);

					return new PSCredential(cred.User, x);
				}
				return null;
			#endif
		}

		public override PSHostRawUserInterface RawUI {
			get { return rawUI; }
		}

		#if noConsole
		private string _ib_message;
		#endif

		public override string ReadLine() {
			#if !noConsole
				return Console.ReadLine();
			#else
				string sWert = "";
				if (Input_Box.Show(rawUI.WindowTitle, _ib_message, ref sWert) == DialogResult.OK)
					return sWert;
				#if exitOnCancel
					Environment.Exit(1);
				#endif
				return "";
			#endif
		}

		private System.Security.SecureString getPassword() {
			System.Security.SecureString pwd = new System.Security.SecureString();
			while (true) {
				ConsoleKeyInfo i = Console.ReadKey(true);
				if (i.Key == ConsoleKey.Enter) {
					Console.WriteLine();
					break;
				} else if (i.Key == ConsoleKey.Backspace) {
					if (pwd.Length > 0) {
						pwd.RemoveAt(pwd.Length - 1);
						Console.Write("\b \b");
					}
				} else if (i.KeyChar != '\u0000') {
					pwd.AppendChar(i.KeyChar);
					Console.Write("*");
				}
			}
			return pwd;
		}

		public override System.Security.SecureString ReadLineAsSecureString() {
			System.Security.SecureString secstr;
			#if !noConsole
				secstr = getPassword();
			#else
				secstr = new System.Security.SecureString();
				string sWert = "";

				if (Input_Box.Show(rawUI.WindowTitle, _ib_message, ref sWert, true) == DialogResult.OK) {
					foreach(char ch in sWert)
					secstr.AppendChar(ch);
				}
				#if exitOnCancel
				else
					Environment.Exit(1);
				#endif
			#endif
			return secstr;
		}

		// 由 Write-Host 调用
		public override void Write(ConsoleColor foregroundColor, ConsoleColor backgroundColor, string value) {
			#if !noOutput
			#if !noConsole
				ConsoleColor fgc = Console.ForegroundColor, bgc = Console.BackgroundColor;
				Console.ForegroundColor = foregroundColor;
				Console.BackgroundColor = backgroundColor;
				Console.Write(value);
				Console.ForegroundColor = fgc;
				Console.BackgroundColor = bgc;
			#else
				if ((!string.IsNullOrEmpty(value)) && (value != "\n"))
					MessageBox.Show(value, rawUI.WindowTitle);
			#endif
			#endif
		}

		public override void Write(string value) {
			#if !noOutput
			#if !noConsole
				Console.Write(value);
			#else
				if ((!string.IsNullOrEmpty(value)) && (value != "\n"))
					MessageBox.Show(value, rawUI.WindowTitle);
			#endif
			#endif
		}

		// 由 Write-Debug 调用
		public override void WriteDebugLine(string message) {
			#if !noDebug
			#if !noConsole
				WriteLineInternal(DebugForegroundColor, DebugBackgroundColor, string.Format("DEBUG: {0}", message));
			#else
				MessageBox.Show(message, rawUI.WindowTitle, MessageBoxButtons.OK, MessageBoxIcon.Information);
			#endif
			#endif
		}

		// 由 Write-Error 调用
		public override void WriteErrorLine(string value) {
			#if !noError
			#if !noConsole
				if (Console_Info.IsErrorRedirected())
					Console.Error.WriteLine(string.Format("ERROR: {0}", value));
				else
					WriteLineInternal(ErrorForegroundColor, ErrorBackgroundColor, string.Format("ERROR: {0}", value));
			#else
				MessageBox.Show(value, rawUI.WindowTitle, MessageBoxButtons.OK, MessageBoxIcon.Error);
			#endif
			#endif
		}

		internal void WriteErrorRecord(ErrorRecord errorItem) {
			// 特殊处理原生stderr导致的异常
			if (errorItem.Exception is System.Management.Automation.RemoteException) {
				var RemoteException = errorItem.Exception as System.Management.Automation.RemoteException;
				if (RemoteException.SerializedRemoteException == null)
					Console.Error.WriteLine(errorItem.Exception.Message);
				else
					WriteErrorLine(errorItem.ToString());
			}
			else
				WriteErrorLine(errorItem.ToString());
		}

		public override void WriteLine() {
			#if !noOutput
			#if !noConsole
				Console.WriteLine();
			#else
				MessageBox.Show("", rawUI.WindowTitle);
			#endif
			#endif
		}

		public override void WriteLine(ConsoleColor foregroundColor, ConsoleColor backgroundColor, string value) {
			#if !noOutput
			#if !noConsole
				// 上色本身可能因宿主控制台状态异常而抛错（比如句柄暂时无效）；上色失败也不能让这行内容干脆不出现。
				try {
					ConsoleColor fgc = Console.ForegroundColor, bgc = Console.BackgroundColor;
					Console.ForegroundColor = foregroundColor;
					Console.BackgroundColor = backgroundColor;
					Console.WriteLine(value);
					Console.ForegroundColor = fgc;
					Console.BackgroundColor = bgc;
				}
				catch {
					Console.WriteLine(value);
				}
			#else
				if ((!string.IsNullOrEmpty(value)) && (value != "\n"))
					MessageBox.Show(value, rawUI.WindowTitle);
			#endif
			#endif
		}

		#if !noConsole
		private void WriteLineInternal(ConsoleColor foregroundColor, ConsoleColor backgroundColor, string value) {
			// 同上：ERROR/WARNING/DEBUG 走这条路，上色失败绝不能让失败原因本身消失（issue 60）。
			try {
				ConsoleColor fgc = Console.ForegroundColor, bgc = Console.BackgroundColor;
				Console.ForegroundColor = foregroundColor;
				Console.BackgroundColor = backgroundColor;
				Console.WriteLine(value);
				Console.ForegroundColor = fgc;
				Console.BackgroundColor = bgc;
			}
			catch {
				Console.WriteLine(value);
			}
		}
		#endif

		// 由 Write-Output 调用
		public override void WriteLine(string value) {
			#if !noOutput
			#if !noConsole
				Console.WriteLine(value);
			#else
				if ((!string.IsNullOrEmpty(value)) && (value != "\n"))
					MessageBox.Show(value, rawUI.WindowTitle);
			#endif
			#endif
		}

		#if noConsole
		public Progress_Form pf;
		#endif
		public override void WriteProgress(long sourceId, ProgressRecord record) {
			#if noConsole
			if (pf == null) {
				if (record.RecordType == ProgressRecordType.Completed) return;
				pf = new Progress_Form(rawUI.WindowTitle, ProgressForegroundColor);
				pf.Show();
			}
			pf.Update(record);
			if (record.RecordType == ProgressRecordType.Completed) {
				if (pf.GetCount() == 0) pf = null;
			}
			#else
			if (!Console_Info.IsOutputRedirected()) {// 标准输出被重定向时不写进度条。
				// 用于开启进度指示器的 OSC 序列
				// https://github.com/microsoft/terminal/issues/6700
				if(Console_Info.IsVirtualTerminalSupported()){
					if (record.RecordType == ProgressRecordType.Completed)//结束进度指示器
						Console.Write("\x1b]9;4;0\x1b\\");
					else {
						int percentComplete = record.PercentComplete;
						// Write-Progress 允许负数完成百分比，但不得大于 100，而 OSC 序列限制在 0 到 100。
						if (percentComplete < 0)
							percentComplete = 0;
						Console.Write(string.Format("\x1b]9;4;1;{0}\x1b\\", percentComplete));
					}
				}
			}
			#endif
		}

		// 由 Write-Verbose 调用
		public override void WriteVerboseLine(string message) {
			#if !noVerbose
			#if !noConsole
			WriteLineInternal(VerboseForegroundColor, VerboseBackgroundColor, string.Format("VERBOSE: {0}", message));
			#else
			MessageBox.Show(message, rawUI.WindowTitle, MessageBoxButtons.OK, MessageBoxIcon.Information);
			#endif
			#endif
		}

		// 由 Write-Warning 调用
		public override void WriteWarningLine(string message) {
			#if !noWarning
				#if !noConsole
					WriteLineInternal(WarningForegroundColor, WarningBackgroundColor, string.Format("WARNING: {0}", message));
				#else
					MessageBox.Show(message, rawUI.WindowTitle, MessageBoxButtons.OK, MessageBoxIcon.Warning);
				#endif
			#endif
		}
	}

	internal class PSRunnerHost: PSHost {
		private readonly PSRunnerInterface parent;
		private readonly PSRunnerUI _ui;

		private readonly CultureInfo originalCultureInfo = System.Threading.Thread.CurrentThread.CurrentCulture;

		private readonly CultureInfo originalUICultureInfo = System.Threading.Thread.CurrentThread.CurrentUICulture;

		private Guid _myId = Guid.NewGuid();

		public PSRunnerHost(PSRunnerInterface app, PSRunnerUI ui) {
			this.parent = app;
			this._ui = ui;
		}

		public class ConsoleColorProxy {
			private readonly PSRunnerUI _ui;

			public ConsoleColorProxy(PSRunnerUI ui) {
				if (ui == null) throw new ArgumentNullException("ui");
				_ui = ui;
			}

			public ConsoleColor ErrorForegroundColor {
				get { return _ui.ErrorForegroundColor; }
				set { _ui.ErrorForegroundColor = value; }
			}

			public ConsoleColor ErrorBackgroundColor {
				get { return _ui.ErrorBackgroundColor; }
				set { _ui.ErrorBackgroundColor = value; }
			}

			public ConsoleColor WarningForegroundColor {
				get { return _ui.WarningForegroundColor; }
				set { _ui.WarningForegroundColor = value; }
			}

			public ConsoleColor WarningBackgroundColor {
				get { return _ui.WarningBackgroundColor; }
				set { _ui.WarningBackgroundColor = value; }
			}

			public ConsoleColor DebugForegroundColor {
				get { return _ui.DebugForegroundColor; }
				set { _ui.DebugForegroundColor = value; }
			}

			public ConsoleColor DebugBackgroundColor {
				get { return _ui.DebugBackgroundColor; }
				set { _ui.DebugBackgroundColor = value; }
			}

			public ConsoleColor VerboseForegroundColor {
				get { return _ui.VerboseForegroundColor; }
				set { _ui.VerboseForegroundColor = value; }
			}

			public ConsoleColor VerboseBackgroundColor {
				get { return _ui.VerboseBackgroundColor; }
				set { _ui.VerboseBackgroundColor = value; }
			}

			public ConsoleColor ProgressForegroundColor {
				get { return _ui.ProgressForegroundColor; }
				set { _ui.ProgressForegroundColor = value; }
			}

			public ConsoleColor ProgressBackgroundColor {
				get { return _ui.ProgressBackgroundColor; }
				set { _ui.ProgressBackgroundColor = value; }
			}
		}

		public override PSObject PrivateData {
			get {
				if (_ui == null) return null;
				return _consoleColorProxy ?? (_consoleColorProxy = PSObject.AsPSObject(new ConsoleColorProxy(_ui)));
			}
		}

		private PSObject _consoleColorProxy;

		public override System.Globalization.CultureInfo CurrentCulture {
			get { return this.originalCultureInfo; }
		}

		public override System.Globalization.CultureInfo CurrentUICulture {
			get { return this.originalUICultureInfo; }
		}

		public override Guid InstanceId {
			get { return this._myId; }
		}

		public override string Name {
			get { return "PSEXE"; }
		}

		public override PSHostUserInterface UI {
			get { return _ui; }
		}

		public override Version Version {
			get { return new Version(0, 0, 0, 0); }
		}

		public override void EnterNestedPrompt() {}

		public override void ExitNestedPrompt() {}

		public override void NotifyBeginApplication() {}

		public override void NotifyEndApplication() {}

		public override void SetShouldExit(int exitCode) {
			this.parent.ShouldExit = true;
			this.parent.ExitCode = exitCode;
		}
	}

	internal interface PSRunnerInterface {
		bool ShouldExit {get;set;}
		int ExitCode {get;set;}
	}

	internal class PSRunner: PSRunnerInterface {
		// 启动计时。需要计时时用 ps12exe -StartupTiming 编译，计时输出走 stderr。
		#if StartupTiming
		internal static System.Diagnostics.Stopwatch TimerSw = System.Diagnostics.Stopwatch.StartNew();
		#endif
		[System.Diagnostics.Conditional("StartupTiming")]
		internal static void TimerMark(string s) {
			#if StartupTiming
				System.Console.Error.WriteLine("[timing] " + s + ": " + TimerSw.Elapsed.TotalMilliseconds.ToString("F1") + " ms");
			#endif
		}

		private bool shouldExit;

		private int exitCode;
		public bool Inited;

		public bool ShouldExit {
			get { return this.shouldExit; }
			set { this.shouldExit = value; }
		}

		public int ExitCode {
			get { return this.exitCode; }
			set { this.exitCode = value; }
		}

		public PSRunnerUI ui;
		public PSRunnerHost host;
		public Runspace PSRunSpace;
		public PowerShell pwsh;

		public PSRunner() {
			TimerMark("ctor:enter");
			this.shouldExit = false;
			this.exitCode = 0;
			this.ui = new PSRunnerUI();
			TimerMark("ctor:ui");
			this.host = new PSRunnerHost(this, ui);
			#if Pwsh20
				this.PSRunSpace = RunspaceFactory.CreateRunspace(host);
			#else
				// 完整默认 ISS（含 Utility/Management 等内置管理单元）：自身创建稍慢，但首个 cmdlet 调用不必再走模块自动发现。CreateDefault2 的轻量 ISS 会把这份开销推迟到第一管道命令，对 hello world 实测反而慢约 70ms（见 tools/Benchmark）。
				InitialSessionState iss = InitialSessionState.CreateDefault();
				this.PSRunSpace = RunspaceFactory.CreateRunspace(host, iss);
			#endif
			TimerMark("ctor:runspace-create");
			this.PSRunSpace.ApartmentState = System.Threading.ApartmentState.$threadingModel;
			this.PSRunSpace.Open();
			TimerMark("ctor:runspace-open");
			this.pwsh = PowerShell.Create();
			this.pwsh.Runspace = PSRunSpace;
			TimerMark("ctor:pwsh-create");
			#if CoreHost
				string exepath = System.Environment.ProcessPath;
			#else
				string exepath = Assembly.GetEntryAssembly().Location;
			#endif
			Assembly executingAssembly = Assembly.GetExecutingAssembly();
			string script;
			// 脚本以 main.ps1 资源内嵌在负载程序集里；负载本身在打包时会被整体压缩。
			using (Stream scriptstream = executingAssembly.GetManifestResourceStream("main.ps1")) {
				using (var scriptreader = new StreamReader(scriptstream, Encoding.UTF8)) {
					script = scriptreader.ReadToEnd();
					this.PSRunSpace.SessionStateProxy.SetVariable("PSEXEscript", script);
				}
			}
			TimerMark("ctor:read-script");
			script = "function PSEXEMainFunction{"+script+"}";
			#if Pwsh20
				this.pwsh.AddScript(script);
			#else
			{
				Token[] tokens;
				ParseError[] errors;
				ScriptBlockAst AST = Parser.ParseInput(script, exepath, out tokens, out errors);
				TimerMark("ctor:parse");
				this.PSRunSpace.SessionStateProxy.SetVariable("PSEXEIniter", AST.GetScriptBlock());
				TimerMark("ctor:getscriptblock");
				if(errors.Length > 0)
					throw new System.InvalidProgramException(errors[0].Message);
				this.pwsh.AddScript(".$PSEXEIniter");
			}
			#endif
			TimerMark("ctor:done");
		}
		public void Dispose() {
			if (pwsh != null) pwsh.Dispose();
			if (PSRunSpace != null) {
				PSRunSpace.Close();
				PSRunSpace.Dispose();
			}
			host = null;
			ui = null;

			GC.SuppressFinalize(this);
		}
		//基础初始化
		public static void BaseInit() {
			#if UNICODEEncoding && !noConsole
			System.Console.OutputEncoding = new System.Text.UnicodeEncoding();
			#endif
			#if UTF8Encoding && !noConsole
			System.Console.OutputEncoding = new System.Text.UTF8Encoding();
			#endif

			#if culture
			System.Threading.Thread.CurrentThread.CurrentCulture = System.Globalization.CultureInfo.GetCultureInfo("$lcid");
			System.Threading.Thread.CurrentThread.CurrentUICulture = System.Globalization.CultureInfo.GetCultureInfo("$lcid");
			#endif

			#if !noVisualStyles && noConsole
			Application.EnableVisualStyles();
			#endif

			FixModulePath();
		}

		// 把自己的模块目录前置；这里做同样的事，保证轻量 ISS 下命令仍能正确自动加载（issue 61）。
		static void FixModulePath() {
			try {
				string docs = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
				string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
				string systemRoot = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
				string[] preferred = new string[] {
					string.IsNullOrEmpty(docs) ? null : Path.Combine(Path.Combine(docs, "WindowsPowerShell"), "Modules"),
					string.IsNullOrEmpty(programFiles) ? null : Path.Combine(Path.Combine(programFiles, "WindowsPowerShell"), "Modules"),
					string.IsNullOrEmpty(systemRoot) ? null : Path.Combine(Path.Combine(systemRoot, Path.Combine("System32", Path.Combine("WindowsPowerShell", Path.Combine("v1.0", "Modules"))))),
				};
				List<string> parts = new List<string>();
				foreach (string path in preferred) {
					if (!string.IsNullOrEmpty(path) && !parts.Contains(path)) parts.Add(path);
				}
				string existing = Environment.GetEnvironmentVariable("PSModulePath");
				if (!string.IsNullOrEmpty(existing)) {
					foreach (string path in existing.Split(';')) {
						if (!string.IsNullOrEmpty(path) && !parts.Contains(path)) parts.Add(path);
					}
				}
				Environment.SetEnvironmentVariable("PSModulePath", string.Join(";", parts.ToArray()));
			} catch {
				// 模块路径修正失败不应影响启动
			}
		}
	}
	static class PSRunnerEntry {
		static PSRunner me;

		#if ScriptHasParam
		// 把命令行参数当 PowerShell 数据(PSD)解析：只接受字面量（字符串/数字/bool/null/数组/哈希表），
		// 任何表达式或命令都视为普通字符串。解析出的对象直接通过变量传入，不再作为文本进入命令行，
		// 因此参数内容不会被当作 PowerShell 脚本求值。
		// 例外：允许到安全类型的安全转换（如 [int]'5'、[hashtable]@{}、[ordered]@{}）。
		static readonly Dictionary<string, Type> PsdSafeCastTypes = new Dictionary<string, Type>(StringComparer.OrdinalIgnoreCase) {
			{ "int", typeof(int) }, { "int32", typeof(int) }, { "system.int32", typeof(int) },
			{ "long", typeof(long) }, { "int64", typeof(long) }, { "system.int64", typeof(long) },
			{ "short", typeof(short) }, { "int16", typeof(short) },
			{ "byte", typeof(byte) }, { "sbyte", typeof(sbyte) },
			{ "uint", typeof(uint) }, { "uint32", typeof(uint) },
			{ "ulong", typeof(ulong) }, { "uint64", typeof(ulong) }, { "ushort", typeof(ushort) },
			{ "single", typeof(float) }, { "float", typeof(float) },
			{ "double", typeof(double) }, { "system.double", typeof(double) },
			{ "decimal", typeof(decimal) },
			{ "string", typeof(string) }, { "system.string", typeof(string) }, { "char", typeof(char) },
			{ "bool", typeof(bool) }, { "boolean", typeof(bool) },
			{ "hashtable", typeof(System.Collections.Hashtable) }, { "system.collections.hashtable", typeof(System.Collections.Hashtable) },
			{ "array", typeof(object[]) }, { "object[]", typeof(object[]) }
		};
		static bool TryParsePsdValue(ExpressionAst expression, out object value) {
			value = null;
			if (expression is StringConstantExpressionAst) {
				value = ((StringConstantExpressionAst)expression).Value;
				return true;
			}
			if (expression is ConstantExpressionAst) {
				value = ((ConstantExpressionAst)expression).Value;
				return true;
			}
			ConvertExpressionAst convert = expression as ConvertExpressionAst;
			if (convert != null) {
				if (convert.Child == null || convert.Type == null || convert.Type.TypeName == null) return false;
				string castName = convert.Type.TypeName.Name;
				if (castName == null) return false;
				// [ordered]@{}：与 PowerShell 一样保留键顺序
				if (castName.Equals("ordered", StringComparison.OrdinalIgnoreCase) ||
					castName.Equals("ordereddictionary", StringComparison.OrdinalIgnoreCase) ||
					castName.Equals("System.Collections.Specialized.OrderedDictionary", StringComparison.OrdinalIgnoreCase)) {
					HashtableAst orderedSource = convert.Child as HashtableAst;
					if (orderedSource == null) return false;
					System.Collections.Specialized.OrderedDictionary ordered = new System.Collections.Specialized.OrderedDictionary(StringComparer.OrdinalIgnoreCase);
					foreach (var pair in orderedSource.KeyValuePairs) {
						object key, item;
						string keyText;
						if (!TryParsePsdValue(pair.Item1, out key) || (keyText = key as string) == null) return false;
						if (!TryParsePsdStatement(pair.Item2, out item)) return false;
						ordered[keyText] = item;
					}
					value = ordered;
					return true;
				}
				Type castType;
				if (!PsdSafeCastTypes.TryGetValue(castName, out castType)) return false;
				object converted;
				if (!TryParsePsdValue(convert.Child, out converted)) return false;
				try {
					value = LanguagePrimitives.ConvertTo(converted, castType, CultureInfo.InvariantCulture);
					return true;
				}
				catch {
					return false;
				}
			}
			VariableExpressionAst variable = expression as VariableExpressionAst;
			if (variable != null) {
				switch (variable.VariablePath.UserPath) {
					case "true": value = true; return true;
					case "false": value = false; return true;
					case "null": return true;
				}
				return false;
			}
			UnaryExpressionAst unary = expression as UnaryExpressionAst;
			if (unary != null && unary.TokenKind == TokenKind.Minus) {
				object inner;
				if (!TryParsePsdValue(unary.Child, out inner)) return false;
				if (inner is int) { value = -(int)inner; return true; }
				if (inner is long) { value = -(long)inner; return true; }
				if (inner is double) { value = -(double)inner; return true; }
				if (inner is decimal) { value = -(decimal)inner; return true; }
				return false;
			}
			ArrayLiteralAst arrayLiteral = expression as ArrayLiteralAst;
			if (arrayLiteral != null) {
				List<object> items = new List<object>();
				foreach (ExpressionAst element in arrayLiteral.Elements) {
					object item;
					if (!TryParsePsdValue(element, out item)) return false;
					items.Add(item);
				}
				value = items.ToArray();
				return true;
			}
			ArrayExpressionAst arrayExpression = expression as ArrayExpressionAst;
			if (arrayExpression != null) {
				if (arrayExpression.SubExpression == null || arrayExpression.SubExpression.Statements.Count != 1) return false;
				return TryParsePsdStatement(arrayExpression.SubExpression.Statements[0], out value);
			}
			HashtableAst hashtable = expression as HashtableAst;
			if (hashtable != null) {
				System.Collections.Hashtable result = new System.Collections.Hashtable(StringComparer.OrdinalIgnoreCase);
				foreach (var pair in hashtable.KeyValuePairs) {
					object key, item;
					string keyText;
					if (!TryParsePsdValue(pair.Item1, out key) || (keyText = key as string) == null) return false;
					if (!TryParsePsdStatement(pair.Item2, out item)) return false;
					result[keyText] = item;
				}
				value = result;
				return true;
			}
			return false;
		}
		// 哈希表的值在 AST 里是语句（PipelineAst），取其中的表达式再按 PSD 解析
		static bool TryParsePsdStatement(StatementAst statement, out object value) {
			value = null;
			PipelineAst pipeline = statement as PipelineAst;
			if (pipeline == null || pipeline.PipelineElements.Count != 1)
				return false;
			CommandExpressionAst expression = pipeline.PipelineElements[0] as CommandExpressionAst;
			if (expression == null)
				return false;
			return TryParsePsdValue(expression.Expression, out value);
		}
		static bool TryParsePsd(string text, out object value, out bool explicitCast) {
			value = null;
			explicitCast = false;
			Token[] tokens;
			ParseError[] errors;
			ScriptBlockAst ast = Parser.ParseInput(text, out tokens, out errors);
			if (errors.Length > 0 || ast.EndBlock == null || ast.EndBlock.Statements.Count != 1)
				return false;
			PipelineAst pipeline = ast.EndBlock.Statements[0] as PipelineAst;
			if (pipeline == null || pipeline.PipelineElements.Count != 1)
				return false;
			CommandExpressionAst commandExpression = pipeline.PipelineElements[0] as CommandExpressionAst;
			if (commandExpression == null)
				return false;
			explicitCast = commandExpression.Expression is ConvertExpressionAst;
			return TryParsePsdValue(commandExpression.Expression, out value);
		}
		#endif

		// EXE 主入口
		[$threadingModelThread]
		private static int Main(string[] args) {
			#if StartupTiming
				PSRunner.TimerSw.Restart();
			#endif
			PSRunner.TimerMark("main:enter");
			PSRunner.BaseInit();
			PSRunner.TimerMark("main:baseinit");
			me = new PSRunner();
			PSRunner.TimerMark("main:ctor-done");
			System.Threading.ManualResetEvent mre = new System.Threading.ManualResetEvent(false);

			try {
				#if !noConsole
				Console.CancelKeyPress += (object sender, ConsoleCancelEventArgs eventargs) => {
					try {
						me.pwsh.BeginStop((_) => {
							mre.Set();
							eventargs.Cancel = true;
						}, null);
					} catch {
						// 忽略，因为正在关闭
					}
				};
				#endif

				#if ScriptHasParam
				int psdIndex = 0;
				#endif
				for(int i = 0; i < args.Length; i++) {
					if (Regex.IsMatch(args[i], @"^(-|\$)\w*$"))
						continue;
					#if ScriptHasParam
					// 脚本有 param 块时，显式安全转换或表/数组类参数值按 PSD 数据解析成对象，再用变量传入，
					// 避免作为文本被求值；其余值仍按字符串传递，保持数字/字符串的原有绑定行为。
					object psdValue;
					bool explicitCast;
					if (TryParsePsd(args[i], out psdValue, out explicitCast) &&
						(explicitCast || psdValue is System.Collections.IDictionary || psdValue is System.Array)) {
						string psdVar = "PSEXEArg" + (psdIndex++);
						me.pwsh.Runspace.SessionStateProxy.SetVariable(psdVar, psdValue);
						args[i] = "$" + psdVar;
						continue;
					}
					#endif
					args[i] = "\'"+args[i].Replace("'", "''")+"\'";
				}

				#if ReadInput
					// 仅当编译期检测到脚本顶层使用 $input 时（ReadInput）才读取重定向的标准输入：否则保留原始 stdin，且不在启动时等待 stdin（issue 62）。
					PSDataCollection<string> colInput = new PSDataCollection<string> ();
					if (Console_Info.IsInputRedirected()) { // 读取标准输入
						string sItem;
						while ((sItem = Console.ReadLine()) != null) { // 添加到 powershell 管道
							colInput.Add(sItem);
						}
					}
					colInput.Complete();

					me.pwsh.Runspace.SessionStateProxy.SetVariable("PSEXEInput", colInput);
					me.pwsh.AddScript("$PSEXEInput|PSEXEMainFunction "+String.Join(" ", args));
				#else
					// 脚本顶层不用 $input：完全不带管道输入，也不设置 $PSEXEInput
					me.pwsh.AddScript("PSEXEMainFunction "+String.Join(" ", args));
				#endif
				// Out-Default 走 host UI；勿用 Out-String/输出收集，否则 native 子进程 stdout 会变成管道（非 TTY）
				me.pwsh.AddCommand("Out-Default");
				me.pwsh.Streams.Error.DataAdded += (sender, eventargs) => {
					me.ui.WriteErrorRecord(((PSDataCollection<ErrorRecord>)sender)[eventargs.Index]);
				};
				IAsyncResult asyncResult = me.pwsh.BeginInvoke();
				PSRunner.TimerMark("main:begininvoke");

				System.Threading.WaitHandle[] waitHandles = new System.Threading.WaitHandle[] { mre, asyncResult.AsyncWaitHandle };
				while (System.Threading.WaitHandle.WaitAny(waitHandles, 10) == System.Threading.WaitHandle.WaitTimeout) {
					if (me.ShouldExit) break;
				}

				PSRunner.TimerMark("main:pipeline-completed");
				me.Inited = true;
				me.pwsh.EndInvoke(asyncResult);
				PSRunner.TimerMark("main:endinvoke");
				me.pwsh.Stop();
				PSRunner.TimerMark("main:stop");

				if (me.pwsh.InvocationStateInfo.State == PSInvocationState.Failed)
					me.ui.WriteErrorLine(me.pwsh.InvocationStateInfo.Reason.Message);
			}
			catch (Exception ex) {
				#if !noError
					me.ui.WriteErrorLine(ex.Message);
				#endif
				me.ExitCode = 1;
			}
			finally {
				#if !Pwsh20 // bro wtf
					mre.Dispose();
				#endif
				me.pwsh.Dispose();
			}

			return me.ExitCode;
		}
	}
}
