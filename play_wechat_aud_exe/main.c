#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>

#include "../play_wechat_silk_dll/silk.h"


int main(int argc, char* argv[])
{

	char      bitInFileName[150];
	if (argc < 2) {
		exit(0);
	}
	/* get arguments */
	strcpy(bitInFileName, argv[1]);
	play_audio(bitInFileName, 24000, 1);
	// play_audio(bitInFileName, 24000, 1);

	//silk_to_pcm(bitInFileName, speechOutFileName, 24000, 1);
	return 0;
}