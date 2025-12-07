using System;
using System.Collections;
using System.Collections.Generic;
using System.Management.Automation;
using System.Management.Automation.Runspaces;
using RGiesecke.DllExport;
using System.Runtime.InteropServices;
using System.Threading;

namespace PSRunnerNS {
	partial static class PSRunnerEntry {
		private static readonly object _lock = new object(); // 用于线程同步

		// DllInitChecker
		[$threadingModelThread]
		public static void DllInitChecker() {
			lock (_lock) {
				if (!PSRunner.Inited) {
					PSRunner.BaseInit();
					me = new PSRunner(); // 创建 PSRunner 实例

					// 执行初始化脚本（如果需要）
					PSDataCollection<PSObject> colOutput = new PSDataCollection<PSObject>();

					IAsyncResult asyncResult = me.pwsh.BeginInvoke<PSObject, PSObject>(null, colOutput);

					// 使用 ManualResetEvent 等待 PowerShell 执行完成
					using (ManualResetEvent mre = new ManualResetEvent(false)) {
						ThreadPool.QueueUserWorkItem(_ => {
							try {
								foreach (PSObject outputItem in colOutput)
									Console.WriteLine(outputItem.ToString());
								foreach (ErrorRecord errorItem in me.pwsh.Streams.Error)
									me.ui.WriteErrorRecord(errorItem);

								if (me.pwsh.InvocationStateInfo.State == PSInvocationState.Failed) {
									me.ExitCode = 1;
									me.ui.WriteErrorLine("DllInitChecker failed: " + me.pwsh.InvocationStateInfo.Reason.Message);
								}
							}
							finally {
								mre.Set();
							}
						});
						mre.WaitOne();
						me.pwsh.EndInvoke(asyncResult);
					}

					PSRunner.Inited = true; // 标记为已初始化
				}
			}
		}
		[DllExport("DllExportExample", CallingConvention = CallingConvention.StdCall)]
		public static object DllExportExample(int a, int b) { // 返回类型改为 object
			DllInitChecker();
			object result = null;
			lock (_lock) {
				if (me.ShouldExit)
					throw new InvalidOperationException("PSRunner is exiting."); // 更合适的异常

				//set parameters as variables in psrunspace
				me.PSRunSpace.SessionStateProxy.SetVariable("PSEXEDLLCallIngParameters", new ArrayList { a, b });
				me.pwsh.Commands.Clear(); // 清除之前的命令
				me.pwsh.AddScript("DllExportExample @PSEXEDLLCallIngParameters");

				PSDataCollection<PSObject> colOutput = new PSDataCollection<PSObject>();
				IAsyncResult asyncResult = me.pwsh.BeginInvoke<PSObject, PSObject>(null, colOutput);

				// 使用 ManualResetEvent 等待 PowerShell 执行完成
				using (ManualResetEvent mre = new ManualResetEvent(false)) {
					ThreadPool.QueueUserWorkItem(_ => {
						try {
							foreach (PSObject outputItem in colOutput)
								Console.WriteLine(outputItem.ToString());
							foreach (ErrorRecord errorItem in me.pwsh.Streams.Error)
								me.ui.WriteErrorRecord(errorItem);

							if (me.pwsh.InvocationStateInfo.State == PSInvocationState.Failed)
								throw new InvalidOperationException("DllExportExample failed: " + me.pwsh.InvocationStateInfo.Reason.Message);
						}
						finally {
							mre.Set();
						}
					});

					mre.WaitOne(); // 等待输出处理完成
					me.pwsh.EndInvoke(asyncResult);
				}

				//处理返回值
				if (colOutput.Count == 1)
					result = colOutput[0].BaseObject; //返回实际的值, 而不是PSObject
				else if (colOutput.Count > 1)
					result = colOutput.ToArray(); // 如果有多个输出，返回 PSObject 数组
			}

			return result;
		}
	}
}
