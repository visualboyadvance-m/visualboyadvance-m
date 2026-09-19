// 由 Ingo Karstein 创建的简单 PowerShell 主机（http://blog.karstein-consulting.com），由 Markus Scholtes 重构并支持 GUI。

using System;
using System.Collections.Generic;
using System.Text;
using System.Globalization;
using System.Reflection;
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
	internal static class PSRunnerEntry {
		private static int Main() {
			#if !noOutput
				#if UNICODEEncoding && !noConsole
				System.Console.OutputEncoding = new System.Text.UnicodeEncoding();
				#endif
				#if UTF8Encoding && !noConsole
				System.Console.OutputEncoding = new System.Text.UTF8Encoding();
				#endif

				#if !noVisualStyles && noConsole
				Application.EnableVisualStyles();
				#endif

				#if noConsole
					// 加载 assembly:AssemblyTitle
					AssemblyTitleAttribute titleAttribute = (AssemblyTitleAttribute) Attribute.GetCustomAttribute(Assembly.GetExecutingAssembly(), typeof(AssemblyTitleAttribute));
					string title;
					if (titleAttribute != null)
						title = titleAttribute.Title;
					else
						title = System.AppDomain.CurrentDomain.FriendlyName;
					// 弹窗输出 \ConstResult
					string[] ConstResult = { "$ConstResult" };
					foreach (string item in ConstResult)
						MessageBox.Show(item, title, MessageBoxButtons.OK);
				#else
					// 控制台输出 \ConstResult
					System.Console.WriteLine("$ConstResult");
				#endif
			#endif
			return $ConstExitCodeResult;
		}
	}
}
