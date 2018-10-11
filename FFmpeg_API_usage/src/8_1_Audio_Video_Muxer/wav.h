#pragma once

typedef struct _wave_pcm_hdr {
	char   riff[4];             // = "RIFF"
	int    size_8;              // = FileSize - 8
	char   wav[4];              // = "WAVE"
	char   fmt[4];              // = "fmt"
	int    fmt_size;            // = ��һ���ṹ��Ĵ�С: 16
	short int format_tag;       // = PCM:1
	short int channels;         // = ͨ����:2
	int   samples_per_sec;      // = ������: 8000|6000|11025|44100
	int   avg_bytes_per_sec;    // = ÿ���ֽ����� samples_per_sec * bits_per_sample / 8
	short int  block_align;     // = ÿ��������ֽ���: bits_per_sample / 8
	short int  bits_per_sample; // = ������������ 8|16
	char data[4];               // =  "data"
	int  data_size;             // =  �����ݳ���: FileSize - 44
}wave_pcm_hdr;

