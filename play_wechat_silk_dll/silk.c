/***********************************************************************
Copyright (c) 2006-2012, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, (subject to the limitations in the disclaimer below)
are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Skype Limited, nor the names of specific
contributors, may be used to endorse or promote products derived from
this software without specific prior written permission.
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/
/*****************************/
/* Silk decoder test program */
/*****************************/
#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE    1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SKP_Silk_SDK_API.h"
#include "SKP_Silk_SigProc_FIX.h"
#include "silk.h"
/* Define codec specific settings should be moved to h file */
#define MAX_BYTES_PER_FRAME     1024
#define MAX_INPUT_FRAMES        5
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_LBRR_DELAY          2


#ifdef _SYSTEM_IS_BIG_ENDIAN
/* Function to convert a little endian int16 to a */
/* big endian int16 or vica verca                 */
void swap_endian(
	SKP_int16       vec[],
	SKP_int         len
)
{
	SKP_int i;
	SKP_int16 tmp;
	SKP_uint8 *p1, *p2;
	for (i = 0; i < len; i++) {
		tmp = vec[i];
		p1 = (SKP_uint8 *)&vec[i]; p2 = (SKP_uint8 *)&tmp;
		p1[0] = p2[1]; p1[1] = p2[0];
	}
}
#endif
#if (defined(_WIN32) || defined(_WINCE)) 
#include <windows.h>	/* timer */
#else    // Linux or Mac
#include <sys/time.h>
#endif
#ifdef _WIN32
unsigned long GetHighResolutionTime() /* O: time in usec*/
{
	/* Returns a time counter in microsec	*/
	/* the resolution is platform dependent */
	/* but is typically 1.62 us resolution  */
	LARGE_INTEGER lpPerformanceCount;
	LARGE_INTEGER lpFrequency;
	QueryPerformanceCounter(&lpPerformanceCount);
	QueryPerformanceFrequency(&lpFrequency);
	return (unsigned long)((1000000 * (lpPerformanceCount.QuadPart)) / lpFrequency.QuadPart);
}
#else    // Linux or Mac
unsigned long GetHighResolutionTime() /* O: time in usec*/
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return((tv.tv_sec * 1000000) + (tv.tv_usec));
}
#endif // _WIN32
/* Seed for the random number generator, which is used for simulating packet loss */
static SKP_int32 rand_seed = 1;







//播放的缓冲区
char *play_buffer = NULL;
char *play_buffer_start = NULL;
HANDLE play_event = NULL;
HANDLE decode_event = NULL;


//一次播放的buffer的长度
#define PLAY_SIZE				(1024*sizeof(SKP_int16))


HWAVEOUT        hwo;
WAVEHDR         wh;
WAVEFORMATEX    wfx;
HANDLE          wait;


DWORD WINAPI play_stream_data(LPVOID pParam)
{
	while (1)
	{
		WaitForSingleObject(play_event, INFINITE);
		ResetEvent(play_event);
		wh.lpData = play_buffer_start;
		wh.dwBufferLength = play_buffer - play_buffer_start;
		wh.dwFlags = 0L;
		wh.dwLoops = 1L;
		waveOutPrepareHeader(hwo, &wh, sizeof(WAVEHDR));//准备一个波形数据块用于播放
		waveOutWrite(hwo, &wh, sizeof(WAVEHDR));//在音频媒体中播放第二个函数wh指定的数据
		WaitForSingleObject(wait, INFINITE);//用来检测hHandle事件的信号状态，在某一线程中调用该函数时，线程暂时挂起，如果在挂起的INFINITE毫秒内，线程所等待的对象变为有信号状态，则该函数立即返回
		SetEvent(decode_event);
	}
	return 0;
}

void play_audio(
	char	*srcFile,		/*源文件*/
	int		API_Fs_Hz,		/* I:   Output signal sampling rate in Hertz; 8000/12000/16000/24000 */
	int		verbose			/*是否详细输出日志*/
)
{
	play_buffer = (char *)malloc(PLAY_SIZE);
	memset(play_buffer, 0, PLAY_SIZE);
	play_buffer_start = play_buffer;
	play_event = CreateEvent(NULL, TRUE, 0, NULL);//
	decode_event = CreateEvent(NULL, TRUE, 0, NULL);
	wfx.wFormatTag = WAVE_FORMAT_PCM; //WAVE_FORMAT_PCM;//设置波形声音的格式
	wfx.nChannels = 1;//设置音频文件的通道数量

	if (API_Fs_Hz == 0) {
		wfx.nSamplesPerSec = 24000;//设置每个声道播放和记录时的样本频率
	}
	else {
		wfx.nSamplesPerSec = API_Fs_Hz;//设置每个声道播放和记录时的样本频率
	}
	wfx.nAvgBytesPerSec = 24000;//设置请求的平均数据传输率,单位byte/s。这个值对于创建缓冲大小是很有用的
	wfx.nBlockAlign = 2;//以字节为单位设置块对齐
	wfx.wBitsPerSample = 16;
	wfx.cbSize = 0;//额外信息的大小
	wait = CreateEvent(NULL, 0, 0, NULL);
	waveOutOpen(&hwo, WAVE_MAPPER, &wfx, (DWORD_PTR)wait, 0L, CALLBACK_EVENT);//打开一个给定的波形音频输出装置来进行
	DWORD dwThreadId;

	HANDLE play_thread = CreateThread(NULL, 0, play_stream_data, NULL, 0, &dwThreadId);

	unsigned long tottime, starttime;
	double    filetime;
	size_t    counter;
	SKP_int32 totPackets, i, k;
	SKP_int16 ret, len, tot_len;
	SKP_int16 nBytes;
	SKP_uint8 payload[MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES * (MAX_LBRR_DELAY + 1)];
	SKP_uint8 *payloadEnd = NULL, *payloadToDec = NULL;
	SKP_uint8 FECpayload[MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES], *payloadPtr;
	SKP_int16 nBytesFEC;
	SKP_int16 nBytesPerPacket[MAX_LBRR_DELAY + 1], totBytes;
	SKP_int16 out[((FRAME_LENGTH_MS * MAX_API_FS_KHZ) << 1) * MAX_INPUT_FRAMES], *outPtr;
	FILE      *bitInFile;
	SKP_int32 packetSize_ms = 0;
	SKP_int32 decSizeBytes;
	void      *psDec;
	SKP_float loss_prob;
	SKP_int32 frames, lost;
	SKP_SILK_SDK_DecControlStruct DecControl;
	if (verbose != 0) {
		printf("********** Silk Decoder (Fixed Point) v %s ********************\n", SKP_Silk_SDK_get_version());
		printf("********** Compiled for %d bit cpu *******************************\n", (int)sizeof(void*) * 8);
		printf("Input:                       %s\n", srcFile);
	}
	bitInFile = fopen(srcFile, "rb");
	if (bitInFile == NULL)
	{
		printf("Error: could not open input file %s\n", srcFile);
		return;
	}
	/* default settings */
	loss_prob = 0.0f;
	/* Check Silk header */
	{
		char header_buf[50];
		counter = fread(header_buf, sizeof(char), strlen("#!SILK_V3") + 1, bitInFile);
		header_buf[strlen("#!SILK_V3") + 1] = '\0'; /* Terminate with a null character */
		if (strcmp(header_buf, "\x2#!SILK_V3") != 0) {
			/* Non-equal strings */
			printf("Error: Wrong Header %s\n", header_buf);
			return;
		}
	}
	/* Set the samplingrate that is requested for the output */
	if (API_Fs_Hz == 0) {
		DecControl.API_sampleRate = 24000;
	}
	else {
		DecControl.API_sampleRate = API_Fs_Hz;
	}
	/* Initialize to one frame per packet, for proper concealment before first packet arrives */
	DecControl.framesPerPacket = 1;
	/* Create decoder */
	ret = SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes);
	if (ret) {
		printf("\nSKP_Silk_SDK_Get_Decoder_Size returned %d", ret);
	}
	psDec = malloc(decSizeBytes);
	/* Reset decoder */
	ret = SKP_Silk_SDK_InitDecoder(psDec);
	if (ret) {
		printf("\nSKP_Silk_InitDecoder returned %d", ret);
	}
	totPackets = 0;
	tottime = 0;
	payloadEnd = payload;
	/* Simulate the jitter buffer holding MAX_FEC_DELAY packets */
	for (i = 0; i < MAX_LBRR_DELAY; i++) {
		/* Read payload size */
		counter = fread(&nBytes, sizeof(SKP_int16), 1, bitInFile);
#ifdef _SYSTEM_IS_BIG_ENDIAN
		swap_endian(&nBytes, 1);
#endif
		/* Read payload */
		counter = fread(payloadEnd, sizeof(SKP_uint8), nBytes, bitInFile);
		if ((SKP_int16)counter < nBytes) {
			break;
		}
		nBytesPerPacket[i] = nBytes;
		payloadEnd += nBytes;
		totPackets++;
	}

	long pack_length = 0;// 已经解码的数据的长度

	while (1) {
		/* Read payload size */
		counter = fread(&nBytes, sizeof(SKP_int16), 1, bitInFile);
#ifdef _SYSTEM_IS_BIG_ENDIAN
		swap_endian(&nBytes, 1);
#endif
		if (nBytes < 0 || counter < 1) {
			break;
		}
		/* Read payload */
		counter = fread(payloadEnd, sizeof(SKP_uint8), nBytes, bitInFile);
		if ((SKP_int16)counter < nBytes) {
			break;
		}
		/* Simulate losses */
		rand_seed = SKP_RAND(rand_seed);
		if ((((float)((rand_seed >> 16) + (1 << 15))) / 65535.0f >= (loss_prob / 100.0f)) && (counter > 0)) {
			nBytesPerPacket[MAX_LBRR_DELAY] = nBytes;
			payloadEnd += nBytes;
		}
		else {
			nBytesPerPacket[MAX_LBRR_DELAY] = 0;
		}
		if (nBytesPerPacket[0] == 0) {
			/* Indicate lost packet */
			lost = 1;
			/* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
			payloadPtr = payload;
			for (i = 0; i < MAX_LBRR_DELAY; i++) {
				if (nBytesPerPacket[i + 1] > 0) {
					starttime = GetHighResolutionTime();
					SKP_Silk_SDK_search_for_LBRR(payloadPtr, nBytesPerPacket[i + 1], (i + 1), FECpayload, &nBytesFEC);
					tottime += GetHighResolutionTime() - starttime;
					if (nBytesFEC > 0) {
						payloadToDec = FECpayload;
						nBytes = nBytesFEC;
						lost = 0;
						break;
					}
				}
				payloadPtr += nBytesPerPacket[i + 1];
			}
		}
		else {
			lost = 0;
			nBytes = nBytesPerPacket[0];
			payloadToDec = payload;
		}
		/* Silk decoder */
		outPtr = out;
		tot_len = 0;
		starttime = GetHighResolutionTime();
		if (lost == 0) {
			/* No Loss: Decode all frames in the packet */
			frames = 0;
			do {
				/* Decode 20 ms */
				ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 0, payloadToDec, nBytes, outPtr, &len);
				if (ret) {
					printf("\nSKP_Silk_SDK_Decode returned %d", ret);
				}
				frames++;
				outPtr += len;
				tot_len += len;
				if (frames > MAX_INPUT_FRAMES) {
					/* Hack for corrupt stream that could generate too many frames */
					outPtr = out;
					tot_len = 0;
					frames = 0;
				}
				/* Until last 20 ms frame of packet has been decoded */
			} while (DecControl.moreInternalDecoderFrames);
		}
		else {
			/* Loss: Decode enough frames to cover one packet duration */
			for (i = 0; i < DecControl.framesPerPacket; i++) {
				/* Generate 20 ms */
				ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 1, payloadToDec, nBytes, outPtr, &len);
				if (ret) {
					printf("\nSKP_Silk_Decode returned %d", ret);
				}
				outPtr += len;
				tot_len += len;
			}
		}
		packetSize_ms = tot_len / (DecControl.API_sampleRate / 1000);
		tottime += GetHighResolutionTime() - starttime;
		totPackets++;
		/* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN   
		swap_endian(out, tot_len);
#endif
		// fwrite(out, sizeof(SKP_int16), tot_len, speechOutFile);
		pack_length += sizeof(SKP_int16) * tot_len;
		if (pack_length > PLAY_SIZE / 2)
		{
			SetEvent(play_event);//播放
			WaitForSingleObject(decode_event, INFINITE);
			ResetEvent(decode_event);
			pack_length = 0;
			play_buffer = play_buffer_start;
		}
		memcpy(play_buffer, out, sizeof(SKP_int16) * tot_len);
		play_buffer += sizeof(SKP_int16) * tot_len;

		/* Update buffer */
		totBytes = 0;
		for (i = 0; i < MAX_LBRR_DELAY; i++) {
			totBytes += nBytesPerPacket[i + 1];
		}
		SKP_memmove(payload, &payload[nBytesPerPacket[0]], totBytes * sizeof(SKP_uint8));
		payloadEnd -= nBytesPerPacket[0];
		SKP_memmove(nBytesPerPacket, &nBytesPerPacket[1], MAX_LBRR_DELAY * sizeof(SKP_int16));
		if (verbose != 0) {
			fprintf(stderr, "\rPackets decoded:             %d\n", totPackets);
		}
	}
	/* Empty the recieve buffer */
	for (k = 0; k < MAX_LBRR_DELAY; k++) {
		if (nBytesPerPacket[0] == 0) {
			/* Indicate lost packet */
			lost = 1;
			/* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
			payloadPtr = payload;
			for (i = 0; i < MAX_LBRR_DELAY; i++) {
				if (nBytesPerPacket[i + 1] > 0) {
					starttime = GetHighResolutionTime();
					SKP_Silk_SDK_search_for_LBRR(payloadPtr, nBytesPerPacket[i + 1], (i + 1), FECpayload, &nBytesFEC);
					tottime += GetHighResolutionTime() - starttime;
					if (nBytesFEC > 0) {
						payloadToDec = FECpayload;
						nBytes = nBytesFEC;
						lost = 0;
						break;
					}
				}
				payloadPtr += nBytesPerPacket[i + 1];
			}
		}
		else {
			lost = 0;
			nBytes = nBytesPerPacket[0];
			payloadToDec = payload;
		}
		/* Silk decoder */
		outPtr = out;
		tot_len = 0;
		starttime = GetHighResolutionTime();
		if (lost == 0) {
			/* No loss: Decode all frames in the packet */
			frames = 0;
			do {
				/* Decode 20 ms */
				ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 0, payloadToDec, nBytes, outPtr, &len);
				if (ret) {
					printf("\nSKP_Silk_SDK_Decode returned %d", ret);
				}
				frames++;
				outPtr += len;
				tot_len += len;
				if (frames > MAX_INPUT_FRAMES) {
					/* Hack for corrupt stream that could generate too many frames */
					outPtr = out;
					tot_len = 0;
					frames = 0;
				}
				/* Until last 20 ms frame of packet has been decoded */
			} while (DecControl.moreInternalDecoderFrames);
		}
		else {
			/* Loss: Decode enough frames to cover one packet duration */
			/* Generate 20 ms */
			for (i = 0; i < DecControl.framesPerPacket; i++) {
				ret = SKP_Silk_SDK_Decode(psDec, &DecControl, 1, payloadToDec, nBytes, outPtr, &len);
				if (ret) {
					printf("\nSKP_Silk_Decode returned %d", ret);
				}
				outPtr += len;
				tot_len += len;
			}
		}
		packetSize_ms = tot_len / (DecControl.API_sampleRate / 1000);
		tottime += GetHighResolutionTime() - starttime;
		totPackets++;
		/* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN   
		swap_endian(out, tot_len);
#endif
		// fwrite(out, sizeof(SKP_int16), tot_len, speechOutFile);
		pack_length += sizeof(SKP_int16) * tot_len;
		if (pack_length > PLAY_SIZE / 2)
		{
			SetEvent(play_event);//播放
			WaitForSingleObject(decode_event, INFINITE);
			ResetEvent(decode_event);
			pack_length = 0;
			play_buffer = play_buffer_start;
		}
		memcpy(play_buffer, out, sizeof(SKP_int16) * tot_len);
		play_buffer += sizeof(SKP_int16) * tot_len;

		/* Update Buffer */
		totBytes = 0;
		for (i = 0; i < MAX_LBRR_DELAY; i++) {
			totBytes += nBytesPerPacket[i + 1];
		}
		SKP_memmove(payload, &payload[nBytesPerPacket[0]], totBytes * sizeof(SKP_uint8));
		payloadEnd -= nBytesPerPacket[0];
		SKP_memmove(nBytesPerPacket, &nBytesPerPacket[1], MAX_LBRR_DELAY * sizeof(SKP_int16));
		if (verbose != 0) {
			fprintf(stderr, "\rPackets decoded:              %d", totPackets);
		}
	}

	SetEvent(play_event);//播放
	WaitForSingleObject(decode_event, INFINITE);
	ResetEvent(decode_event);
	TerminateThread(play_thread, 0);
	CloseHandle(play_thread);
	CloseHandle(play_event);
	CloseHandle(decode_event);
	/* Free decoder */
	free(psDec);
	/* Close files */
	fclose(bitInFile);
	filetime = totPackets * 1e-3 * packetSize_ms;
	if (verbose != 0) {
		printf("\nFile length:                 %.3f s", filetime);
		printf("\nTime for decoding:           %.3f s (%.3f%% of realtime)", 1e-6 * tottime, 1e-4 * tottime / filetime);
		printf("\n\n");
	}
	else {
		/* print time and % of realtime */
		printf("%.3f %.3f %d\n", 1e-6 * tottime, 1e-4 * tottime / filetime, totPackets);
	}
	play_buffer = play_buffer_start;
	play_buffer_start = NULL;
	int error = GetLastError();

	free(play_buffer);

	waveOutClose(hwo);
}
