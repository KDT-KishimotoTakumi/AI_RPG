using System;
using System.Runtime.InteropServices;
using System.Text;
using System.IO;
using System.Threading;
public static class GirlsSpireShippingDesktop {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    private struct Startup {
        public int cb;
        public string reserved, desktop, title;
        public int x,y,xs,ys,xc,yc,fill,flags;
        public short show, reserved2;
        public IntPtr reserved3, stdin, stdout, stderr;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct Proc { public IntPtr process, thread; public uint pid, tid; }
    private delegate bool Visitor(IntPtr window, IntPtr data);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    private static extern IntPtr CreateDesktop(string name, IntPtr device, IntPtr mode, uint flags,uint access,IntPtr security);
    [DllImport("user32.dll")] private static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll")] private static extern bool EnumDesktopWindows(IntPtr desktop,Visitor visitor,IntPtr data);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr window,StringBuilder title,int size);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] private static extern bool PostMessage(IntPtr window,uint msg,IntPtr w,IntPtr l);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    private static extern bool CreateProcess(string exe,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr env,string cwd,ref Startup start,out Proc result);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll")] private static extern uint WaitForSingleObject(IntPtr h,uint ms);
    [DllImport("kernel32.dll")] private static extern bool GetExitCodeProcess(IntPtr h,out uint code);
    [DllImport("kernel32.dll")] private static extern bool TerminateProcess(IntPtr h,uint code);
    [StructLayout(LayoutKind.Sequential)] private struct Rect { public int left,top,right,bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct BitmapInfo {
        public uint size; public int width,height; public ushort planes,bits;
        public uint compression, bytes; public int x,y; public uint colors, important;
    }
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr h,out Rect r);
    [DllImport("user32.dll")] private static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] private static extern int ReleaseDC(IntPtr h,IntPtr dc);
    [DllImport("user32.dll")] private static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
    [DllImport("gdi32.dll")] private static extern IntPtr CreateCompatibleDC(IntPtr dc);
    [DllImport("gdi32.dll")] private static extern IntPtr CreateCompatibleBitmap(IntPtr dc,int w,int h);
    [DllImport("gdi32.dll")] private static extern IntPtr SelectObject(IntPtr dc,IntPtr item);
    [DllImport("gdi32.dll")] private static extern bool DeleteObject(IntPtr item);
    [DllImport("gdi32.dll")] private static extern bool DeleteDC(IntPtr dc);
    [DllImport("gdi32.dll")] private static extern int GetDIBits(IntPtr dc,IntPtr bitmap,uint start,uint count,byte[] pixels,ref BitmapInfo info,uint usage);
    private static bool Capture(IntPtr window,string output) {
        if(!GetWindowRect(window,out Rect r)) return false;
        int w=r.right-r.left,h=r.bottom-r.top;
        if(w<300 || h<200 || w>4096 || h>4096) return false;
        IntPtr dc=GetDC(window), mem=IntPtr.Zero, bitmap=IntPtr.Zero;
        if(dc==IntPtr.Zero) return false;
        try {
            mem=CreateCompatibleDC(dc);
            bitmap=CreateCompatibleBitmap(dc,w,h);
            if(mem==IntPtr.Zero || bitmap==IntPtr.Zero) return false;
            var prev=SelectObject(mem,bitmap);
            if(prev==IntPtr.Zero || prev==new IntPtr(-1)) return false;
            bool printed=PrintWindow(window,mem,2);
            SelectObject(mem,prev);
            if(!printed) return false;
            var data=new byte[w*h*4];
            var info=new BitmapInfo{size=40,width=w,height=-h,planes=1,bits=32,bytes=(uint)data.Length};
            if(GetDIBits(mem,bitmap,0,(uint)h,data,ref info,0)!=h) return false;
            using(var writer=new BinaryWriter(File.Create(output))) {
                writer.Write((ushort)0x4d42);writer.Write(54+data.Length);
                writer.Write(0);writer.Write(54);writer.Write(40);writer.Write(w);writer.Write(-h);
                writer.Write((ushort)1);writer.Write((ushort)32);writer.Write(0);writer.Write(data.Length);
                writer.Write(0);writer.Write(0);writer.Write(0);writer.Write(0);writer.Write(data);
            }
            return true;
        } finally {
            if(bitmap!=IntPtr.Zero) DeleteObject(bitmap);
            if(mem!=IntPtr.Zero) DeleteDC(mem);
            ReleaseDC(window,dc);
        }
    }
    public sealed class Result {
        public uint Pid { get; set; }
        public string Title { get; set; }
        public bool Captured { get; set; }
        public bool CloseRequested { get; set; }
        public bool Exited { get; set; }
        public uint ExitCode { get; set; }
    }

    public static Result Run(string exe, string cwd, string profile, string desktopName) {
        const uint Waiting = 258;
        var desktop = CreateDesktop(desktopName, IntPtr.Zero, IntPtr.Zero, 0, 0x01ff, IntPtr.Zero);
        if (desktop == IntPtr.Zero)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        try {
            var start = new Startup { cb = Marshal.SizeOf<Startup>(), desktop = desktopName };
            var command = new StringBuilder("\""+exe+"\" -unattended -NoSplash -windowed -ResX=1280 -ResY=900 -UserDir=\""+profile+"/\"");
            if (!CreateProcess(exe, command, IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, cwd, ref start, out Proc p))
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            try {
                var result = new Result { Pid = p.pid, Title = "" };
                IntPtr window = IntPtr.Zero;
                var timer = System.Diagnostics.Stopwatch.StartNew();
                while (timer.Elapsed.TotalSeconds < 45 && WaitForSingleObject(p.process, 0) == Waiting) {
                    EnumDesktopWindows(desktop, (candidate, _) => {
                        GetWindowThreadProcessId(candidate, out uint owner);
                        if (owner != p.pid || !IsWindowVisible(candidate)) return true;
                        var title = new StringBuilder(512);
                        GetWindowText(candidate, title, title.Capacity);
                        var name = title.ToString().Trim();
                        if (name != "GirlsSpire" && name != "Girls Spire" && name != "Spiral RPG") return true;
                        window = candidate;
                        result.Title = name;
                        return false;
                    }, IntPtr.Zero);
                    if (window != IntPtr.Zero) break;
                    Thread.Sleep(250);
                }
                if (window != IntPtr.Zero) {
                    Thread.Sleep(4500);
                    // 描画が応答しなくても本体の終了処理へ進む。
                    var shot = System.Threading.Tasks.Task.Run(() => Capture(window, Path.Combine(profile, "title.bmp")));
                    result.Captured = shot.Wait(5000) && shot.Result;
                    GetWindowThreadProcessId(window, out uint owner);
                    if (owner == p.pid) result.CloseRequested = PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
                }
                result.Exited = WaitForSingleObject(p.process, result.CloseRequested ? 15000u : 0u) == 0;
                if (!GetExitCodeProcess(p.process, out uint exitCode)) exitCode = uint.MaxValue;
                result.ExitCode = exitCode;
                return result;
            } finally {
                // ここで扱うのはCreateProcessが返したこの試験のハンドルだけ。
                try {
                    if (WaitForSingleObject(p.process, 0) == Waiting) {
                        if (!TerminateProcess(p.process, 125) || WaitForSingleObject(p.process, 10000) != 0)
                            throw new InvalidOperationException("試験プロセスの終了を確認できません。");
                    }
                } finally {
                    CloseHandle(p.thread);
                    CloseHandle(p.process);
                }
            }
        } finally {
            CloseDesktop(desktop);
        }
    }
}