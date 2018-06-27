#pragma once
// silk.h  
#pragma comment(lib, "winmm.lib")
#ifdef __cplusplus  
extern "C" {  // only need to export C interface if  
			  // used by C++ source code  
#endif  

	__declspec(dllimport) void play_audio(
		char	*srcFile,		/*Դ�ļ�*/
		int		API_Fs_Hz,		/* I:   Output signal sampling rate in Hertz; 8000/12000/16000/24000 */
		int		verbose			/*�Ƿ���ϸ�����־*/
	);

#ifdef __cplusplus  
}
#endif 

