using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace play_wechat_csharp
{
    class Program
    {
        [DllImport("silk.dll", CharSet = CharSet.Auto, CallingConvention = CallingConvention.Cdecl)]
        static extern void play_audio(
           IntPtr srcFile,      /*源文件*/
           int API_Fs_Hz,      /* I:   Output signal sampling rate in Hertz; 8000/12000/16000/24000 */
           int verbose         /*是否详细输出日志*/
        );

        static void Main(string[] args)
        {
            if (args.Count() < 1)
            {
                return;
            }
            IntPtr src = Marshal.StringToHGlobalAnsi(args[0]);
            play_audio(src, 18000, 1);
        }
    }
}
