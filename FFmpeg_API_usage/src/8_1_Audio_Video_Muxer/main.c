#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "close_media.h"

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")

wave_pcm_hdr default_wav_hdr = {
	{ 'R','I','F','F' },
	0,
	{ 'W', 'A', 'V', 'E' },
	{ 'f', 'm', 't', ' ' },
	16,
	1,       // pcm: 1
	2,       // channels: 2
	44100,   // ������
	88200,   // ÿ���ֽ���: 44100 * 16  / 8
	2,       // 16 / 8
	16,      // ÿ��������ռ�ñ�����
	{ 'd', 'a', 't', 'a' },
	0
};

int main(int argc, char *argv[]) {
	const char *filename = "test.flv";

	int have_video = 0, have_audio = 0;
	int encode_video = 0, encode_audio = 0;
	AVDictionary *opt = NULL;
	int ret = -1;

	//1.ע������API(��װ�������װ����Э��)
	av_register_all();


	//2.����AVFormatContext
	AVFormatContext *oc;
	AVOutputFormat *out_fmt;
	avformat_alloc_output_context2(&oc, NULL, "flv", filename);
	if (!oc) {
		printf("could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}

	if (!oc) {
		printf("alloc output context failed.\n");
		return 1;
	}

	out_fmt = oc->oformat;
	printf("out_fmt: %s.\n", out_fmt->name);

	//3. �����Ƶ���stream
	OutputStream video_st = {0};
	AVCodec *video_codec;
	if (out_fmt->video_codec != AV_CODEC_ID_NONE) {
		//21: AV_CODEC_ID_FLV1,
		printf("out_fmt->video_codec: %s.\n", avcodec_get_name(out_fmt->video_codec));
		
		add_stream(&video_st, oc, &video_codec, out_fmt->video_codec);
		have_video = 1;
		encode_video = 1;
	}


	//4. �����Ƶ���stream
	OutputStream audio_st = {0};
	AVCodec *audio_codec;
	if (out_fmt->audio_codec != AV_CODEC_ID_NONE) {
		//86017: AV_CODEC_ID_MP3,
		printf("out_fmt->audio_codec: %s.\n", avcodec_get_name(out_fmt->audio_codec));

		add_stream(&audio_st, oc, &audio_codec, out_fmt->audio_codec);
		have_audio = 1;
		encode_audio = 1;
	}


	//5. ����Ƶ,�������Ҫ�ı�����buffer
	if (have_video) {
		open_video(oc, video_codec, &video_st, opt);
		//��������������װ��
		ret = avcodec_parameters_from_context(video_st.st->codecpar, video_st.st->codec);
		if (ret < 0) {
			printf("could not copy the stream parameters.\n");
			return -1;
		}
	}
	
	//6. ����Ƶ,�����Ҫ����Ƶ����֡buffer
	if (have_video) {
		open_audio(oc, audio_codec, &audio_st, opt);
		//copy the stream parameters to the muxer
		ret = avcodec_parameters_from_context(audio_st.st->codecpar, audio_st.st->codec);
		if (ret < 0) {
			printf("could not copy the stream parameters.\n");
			exit(1);
		}
	}

	//�����װ��ʽ
	av_dump_format(oc, 0, filename, 1);

	//7. ������ļ�
	if (!(out_fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf("could not open %s : %s\n", filename, av_err2str(ret));
			return 1;
		}
	}

	//8.д���ļ�ͷ
	ret = avformat_write_header(oc, NULL);
	if (ret < 0) {
		printf("Error occured when opening output file: %s.\n", av_err2str(ret));
		return 1;
	}


	//9.д��֡����
	static int cnt = 0;
	audio_st.fp_out = fopen("out.wav", "w+");
	audio_st.wav_hdr = default_wav_hdr;
	fwrite( &audio_st.wav_hdr, sizeof(wave_pcm_hdr), 1, audio_st.fp_out);
	if (audio_st.fp_out == NULL) {
		printf("open out.pcm failed: %s.\n", strerror(errno));
	}

	video_st.fp_out = fopen("out.yuv", "w+");
	if (video_st.fp_out == NULL) {
		printf("open out.yuv failed: %s.\n", strerror(errno));
	}

	while (encode_video || encode_audio) {
		if (encode_video && (!encode_audio || av_compare_ts(video_st.next_pts, video_st.st->codec->time_base, 
			      audio_st.next_pts, audio_st.st->codec->time_base) <= 0)) {
			// write video frame
			encode_video != write_video_frame(oc, &video_st);
		}
		else {
			// write audio frame
			encode_audio != write_audio_frame(oc, &audio_st);
		}

		cnt++;
		if (cnt % 1000 == 0)
			break;
	}

	/*����wav�ļ�ͷ���ݵĴ�С*/
	audio_st.wav_hdr.size_8 += audio_st.wav_hdr.data_size + sizeof(wave_pcm_hdr) - 8;
	/*��������������д��wav�ļ�ͷ��*/
	fseek(audio_st.fp_out, 4, 0);
	fwrite(&audio_st.wav_hdr.size_8, sizeof(audio_st.wav_hdr.size_8), 1, audio_st.fp_out);
	//���ļ�ָ��ƫ�Ƶ��洢data_size��λ��
	fseek(audio_st.fp_out, 40, 0);
	fwrite(&audio_st.wav_hdr.data_size, sizeof(audio_st.wav_hdr.data_size), 1, audio_st.fp_out);
	fclose(audio_st.fp_out);

	fclose(video_st.fp_out);


	//10. д���ļ�β write the trailer
	av_write_trailer(oc);

	//11.�ر�ÿһ��codec
	if (have_audio) {
		close_stream(oc, &audio_st);
	}

	if (have_video) {
		close_stream(oc, &video_st);
	}

	if (!(out_fmt->flags & AVFMT_NOFILE)) {
		avio_closep(&oc->pb);
	}

	//free the stream
	avformat_free_context(oc);

	return 0;
}



