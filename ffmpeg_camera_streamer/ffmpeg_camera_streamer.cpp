/**
 * 
 * ���� Hui Zhang
 * zhanghuicuc@gmail.com
 * �й���ý��ѧ/���ֵ��Ӽ���
 * Communication University of China / Digital TV Technology
 * 
 * ������ʵ���˶�ȡPC������ͷ���ݲ����б������ý�崫�䡣
 *
 */

#define USEFILTER 1

#include <stdio.h>
#include <conio.h>
#include <windows.h>
#define snprintf _snprintf
extern "C"
{
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/mathematics.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#if USEFILTER
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#endif
};


int flush_encoder(AVFormatContext *ifmt_ctx,AVFormatContext *ofmt_ctx, unsigned int stream_index, int framecnt);

int exit_thread = 0;
#if USEFILTER
int filter_change = 1;
const char *filter_descr="null";
const char *filter_mirror = "crop=iw/2:ih:0:0,split[left][tmp];[tmp]hflip[right]; \
												[left]pad=iw*2[a];[a][right]overlay=w";
const char *filter_watermark = "movie=test.jpg[wm];[in][wm]overlay=5:5[out]";
const char *filter_negate = "negate[out]";
const char *filter_edge = "edgedetect[out]";
const char *filter_split4 = "scale=iw/2:ih/2[in_tmp];[in_tmp]split=4[in_1][in_2][in_3][in_4];[in_1]pad=iw*2:ih*2[a];[a][in_2]overlay=w[b];[b][in_3]overlay=0:h[d];[d][in_4]overlay=w:h[out]";
const char *filter_vintage = "curves=vintage";
typedef enum{
	FILTER_NULL =48,
	FILTER_MIRROR ,
	FILTER_WATERMATK,
	FILTER_NEGATE,
	FILTER_EDGE,
	FILTER_SPLIT4,
	FILTER_VINTAGE
}FILTERS;

AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
AVFilter *buffersrc;
AVFilter *buffersink;
AVFrame* picref;
#endif

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{	
#if USEFILTER
	int ch = getchar();
	while (ch != '\n')
	{		
		switch (ch){
			case FILTER_NULL:
			{
				printf("\nnow using null filter\nPress other numbers for other filters:");
				filter_change = 1;
				filter_descr = "null";
				getchar();
				ch = getchar();
				break;
			}
			case FILTER_MIRROR:
			{
				printf("\nnow using mirror filter\nPress other numbers for other filters:");
				filter_change = 1;
				filter_descr = filter_mirror;
				getchar();
				ch = getchar();
				break;
			}
			case FILTER_WATERMATK:
			{
				printf("\nnow using watermark filter\nPress other numbers for other filters:");
				filter_change = 1;
				filter_descr = filter_watermark;
				getchar();
				ch = getchar();
				break;
			}
			case FILTER_NEGATE:
			{
				printf("\nnow using negate filter\nPress other numbers for other filters:");
				filter_change = 1;
				filter_descr = filter_negate;
				getchar();
				ch = getchar();
				break;
			}
			case FILTER_EDGE:
			{
				printf("\nnow using edge filter\nPress other numbers for other filters:");
				filter_change = 1;
				filter_descr = filter_edge;
				getchar();
				ch = getchar();
				break;
			}
			case FILTER_SPLIT4:
			{
				printf("\nnow using split4 filter\nPress other numbers for other filters:");
				filter_change = 1;
				filter_descr = filter_split4;
				getchar();
				ch = getchar();
				break;
			}
			case FILTER_VINTAGE:
			{
				printf("\nnow using vintage filter\nPress other numbers for other filters:");
				filter_change = 1;
				filter_descr = filter_vintage;
				getchar();
				ch = getchar();
				break;
			}
			default:
			{
				getchar();
				ch = getchar();
				break;
			}
		}
#else
	while ((getchar())!='\n')
	{
	;
#endif
	}
	exit_thread = 1;
	return 0;
}

//Show Device  
void show_dshow_device(){
	AVFormatContext *pFmtCtx = avformat_alloc_context();
	AVDictionary* options = NULL;
	av_dict_set(&options, "list_devices", "true", 0);
	AVInputFormat *iformat = av_find_input_format("dshow");
	printf("Device Info=============\n");
	avformat_open_input(&pFmtCtx, "video=dummy", iformat, &options);
	printf("========================\n");
}

#if USEFILTER
static int apply_filters(AVFormatContext *ifmt_ctx)
{
	char args[512];
	int ret;
	AVFilterInOut *outputs = avfilter_inout_alloc();
	if (!outputs)
	{
		printf("Cannot alloc output\n");
		return -1;
	}
	AVFilterInOut *inputs = avfilter_inout_alloc();
	if (!inputs)
	{
		printf("Cannot alloc input\n");
		return -1;
	}

	if (filter_graph)
		avfilter_graph_free(&filter_graph);
	filter_graph = avfilter_graph_alloc();
	if (!filter_graph)
	{
		printf("Cannot create filter graph\n");
		return -1;
	}

	/* buffer video source: the decoded frames from the decoder will be inserted here. */
	snprintf(args, sizeof(args),
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		ifmt_ctx->streams[0]->codec->width, ifmt_ctx->streams[0]->codec->height, ifmt_ctx->streams[0]->codec->pix_fmt,
		ifmt_ctx->streams[0]->time_base.num, ifmt_ctx->streams[0]->time_base.den,
		ifmt_ctx->streams[0]->codec->sample_aspect_ratio.num, ifmt_ctx->streams[0]->codec->sample_aspect_ratio.den);

	ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
		args, NULL, filter_graph);
	if (ret < 0) {
		printf("Cannot create buffer source\n");
		return ret;
	}

	/* buffer video sink: to terminate the filter chain. */
	ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
		NULL, NULL, filter_graph);
	if (ret < 0) {
		printf("Cannot create buffer sink\n");
		return ret;
	}

	/* Endpoints for the filter graph. */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
		&inputs, &outputs, NULL)) < 0)
		return ret;

	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
		return ret;

	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	return 0;
}
#endif

int main(int argc, char* argv[])
{
	AVFormatContext *ifmt_ctx=NULL;
	AVFormatContext *ofmt_ctx;
	AVInputFormat* ifmt;
	AVStream* video_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket *dec_pkt, enc_pkt;
	AVFrame *pframe, *pFrameYUV ;
	struct SwsContext *img_convert_ctx;

	char capture_name[80] = {0};
	char device_name[80] = {0};
	int framecnt=0;
	int videoindex;
	int i;
	int ret;
	HANDLE  hThread;

	const char* out_path = "rtmp://192.168.1.171:1935/live/stream";	 
	int dec_got_frame,enc_got_frame;

	av_register_all();
	//Register Device
	avdevice_register_all();
	avformat_network_init();
#if USEFILTER
	//Register Filter
	avfilter_register_all();
	buffersrc = avfilter_get_by_name("buffer");
	buffersink = avfilter_get_by_name("buffersink");
#endif

	//Show Dshow Device  
	show_dshow_device();
	
	/*printf("\nChoose capture device: ");
	if (gets(capture_name) == 0)
	{
		printf("Error in gets()\n");
		return -1;
	}
	sprintf(device_name, "video=%s", capture_name);*/

	ifmt=av_find_input_format("dshow");
	
	//Set own video device's name
	if (avformat_open_input(&ifmt_ctx, "video=Lenovo EasyCamera", ifmt, NULL) != 0){
		printf("Couldn't open input stream.���޷�����������\n");
		return -1;
	}
	//input initialize
	if (avformat_find_stream_info(ifmt_ctx, NULL)<0)
	{
		printf("Couldn't find stream information.���޷���ȡ����Ϣ��\n");
		return -1;
	}
	videoindex = -1;
	for (i = 0; i<ifmt_ctx->nb_streams; i++)
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoindex = i;
			break;
		}
	if (videoindex == -1)
	{
		printf("Couldn't find a video stream.��û���ҵ���Ƶ����\n");
		return -1;
	}
	if (avcodec_open2(ifmt_ctx->streams[videoindex]->codec, avcodec_find_decoder(ifmt_ctx->streams[videoindex]->codec->codec_id), NULL)<0)
	{
		printf("Could not open codec.���޷��򿪽�������\n");
		return -1;
	}

	//output initialize
	avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_path);
	//output encoder initialize
	pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!pCodec){
		printf("Can not find encoder! (û���ҵ����ʵı�������)\n");
		return -1;
	}
	pCodecCtx=avcodec_alloc_context3(pCodec);
	pCodecCtx->pix_fmt = PIX_FMT_YUV420P;
	pCodecCtx->width = ifmt_ctx->streams[videoindex]->codec->width;
	pCodecCtx->height = ifmt_ctx->streams[videoindex]->codec->height;
	pCodecCtx->time_base.num = 1;
	pCodecCtx->time_base.den = 25;
	pCodecCtx->bit_rate = 400000;
	pCodecCtx->gop_size = 250;
	/* Some formats want stream headers to be separate. */
	if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	//H264 codec param
	//pCodecCtx->me_range = 16;
	//pCodecCtx->max_qdiff = 4;
	//pCodecCtx->qcompress = 0.6;
	pCodecCtx->qmin = 10;
	pCodecCtx->qmax = 51;
	//Optional Param
	pCodecCtx->max_b_frames = 3;
	// Set H264 preset and tune
	AVDictionary *param = 0;
	av_dict_set(&param, "preset", "fast", 0);
	av_dict_set(&param, "tune", "zerolatency", 0);

	if (avcodec_open2(pCodecCtx, pCodec,&param) < 0){
		printf("Failed to open encoder! (��������ʧ�ܣ�)\n");
		return -1;
	}

	//Add a new stream to output,should be called by the user before avformat_write_header() for muxing
	video_st = avformat_new_stream(ofmt_ctx, pCodec);
	if (video_st == NULL){
		return -1;
	}
	video_st->time_base.num = 1;
	video_st->time_base.den = 25;
	video_st->codec = pCodecCtx;

	//Open output URL,set before avformat_write_header() for muxing
	if (avio_open(&ofmt_ctx->pb,out_path, AVIO_FLAG_READ_WRITE) < 0){
	printf("Failed to open output file! (����ļ���ʧ�ܣ�)\n");
	return -1;
	}

	//Show some Information
	av_dump_format(ofmt_ctx, 0, out_path, 1);

	//Write File Header
	avformat_write_header(ofmt_ctx,NULL);

	//prepare before decode and encode
	dec_pkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	//enc_pkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	//camera data has a pix fmt of RGB,convert it to YUV420
#if USEFILTER
#else
	img_convert_ctx = sws_getContext(ifmt_ctx->streams[videoindex]->codec->width, ifmt_ctx->streams[videoindex]->codec->height, 
		ifmt_ctx->streams[videoindex]->codec->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
#endif
	pFrameYUV = av_frame_alloc();
	uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
	
	printf("\n --------call started----------\n");
#if USEFILTER
	printf("\n Press differnet number for different filters:");
	printf("\n 1->Mirror");
	printf("\n 2->Add Watermark");
	printf("\n 3->Negate");
	printf("\n 4->Draw Edge");
	printf("\n 5->Split Into 4");
	printf("\n 6->Vintage");
	printf("\n Press 0 to remove filter\n");
#endif
	printf("\nPress enter to stop...\n");
	hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		MyThreadFunction,       // thread function name
		NULL,          // argument to thread function 
		0,                      // use default creation flags 
		NULL);   // returns the thread identifier 
	
	//start decode and encode
	int64_t start_time=av_gettime();
	while (av_read_frame(ifmt_ctx, dec_pkt) >= 0){	
		if (exit_thread)
			break;
		av_log(NULL, AV_LOG_DEBUG, "Going to reencode the frame\n");
		pframe = av_frame_alloc();
		if (!pframe) {
			ret = AVERROR(ENOMEM);
			return -1;
		}
		//av_packet_rescale_ts(dec_pkt, ifmt_ctx->streams[dec_pkt->stream_index]->time_base,
		//	ifmt_ctx->streams[dec_pkt->stream_index]->codec->time_base);
		ret = avcodec_decode_video2(ifmt_ctx->streams[dec_pkt->stream_index]->codec, pframe,
			&dec_got_frame, dec_pkt);
		if (ret < 0) {
			av_frame_free(&pframe);
			av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
			break;
		}
		if (dec_got_frame){
#if USEFILTER
				pframe->pts = av_frame_get_best_effort_timestamp(pframe);

				if (filter_change)
					apply_filters(ifmt_ctx);
				filter_change = 0;
				/* push the decoded frame into the filtergraph */
				if (av_buffersrc_add_frame(buffersrc_ctx, pframe) < 0) {
					printf("Error while feeding the filtergraph\n");
					break;
				}
				picref = av_frame_alloc();

				/* pull filtered pictures from the filtergraph */
				while (1) {
					ret = av_buffersink_get_frame_flags(buffersink_ctx, picref, 0);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
						break;
					if (ret < 0)
						return ret;

					if (picref) {
						img_convert_ctx = sws_getContext(picref->width, picref->height, (AVPixelFormat)picref->format, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
						sws_scale(img_convert_ctx, (const uint8_t* const*)picref->data, picref->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
						sws_freeContext(img_convert_ctx);
						pFrameYUV->width = picref->width;
						pFrameYUV->height = picref->height;
						pFrameYUV->format = PIX_FMT_YUV420P;
#else
						sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
						pFrameYUV->width = pframe->width;
						pFrameYUV->height = pframe->height;
						pFrameYUV->format = PIX_FMT_YUV420P;
#endif					
						enc_pkt.data = NULL;
						enc_pkt.size = 0;
						av_init_packet(&enc_pkt);
						ret = avcodec_encode_video2(pCodecCtx, &enc_pkt, pFrameYUV, &enc_got_frame);
						av_frame_free(&pframe);
						if (enc_got_frame == 1){
							//printf("Succeed to encode frame: %5d\tsize:%5d\n", framecnt, enc_pkt.size);
							framecnt++;
							enc_pkt.stream_index = video_st->index;

							//Write PTS
							AVRational time_base = ofmt_ctx->streams[videoindex]->time_base;//{ 1, 1000 };
							AVRational r_framerate1 = ifmt_ctx->streams[videoindex]->r_frame_rate;// { 50, 2 };
							AVRational time_base_q = { 1, AV_TIME_BASE };
							//Duration between 2 frames (us)
							int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//�ڲ�ʱ���
							//Parameters
							//enc_pkt.pts = (double)(framecnt*calc_duration)*(double)(av_q2d(time_base_q)) / (double)(av_q2d(time_base));
							enc_pkt.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
							enc_pkt.dts = enc_pkt.pts;
							enc_pkt.duration = av_rescale_q(calc_duration, time_base_q, time_base); //(double)(calc_duration)*(double)(av_q2d(time_base_q)) / (double)(av_q2d(time_base));
							enc_pkt.pos = -1;

							//Delay
							int64_t pts_time = av_rescale_q(enc_pkt.dts, time_base, time_base_q);
							int64_t now_time = av_gettime() - start_time;
							if (pts_time > now_time)
								av_usleep(pts_time - now_time);

							ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
							av_free_packet(&enc_pkt);
						}
#if USEFILTER
						av_frame_unref(picref);
					}
				}		
#endif
		}
		else {
			av_frame_free(&pframe);
		}
		av_free_packet(dec_pkt);
	}
	//Flush Encoder
	ret = flush_encoder(ifmt_ctx,ofmt_ctx,0,framecnt);
	if (ret < 0) {
		printf("Flushing encoder failed\n");
		return -1;
	}

	//Write file trailer
	av_write_trailer(ofmt_ctx);

	//Clean
#if USEFILTER
	if (filter_graph)
		avfilter_graph_free(&filter_graph);
#endif
	if (video_st)
		avcodec_close(video_st->codec);
	av_free(out_buffer);
	avio_close(ofmt_ctx->pb);
	avformat_free_context(ifmt_ctx);
	avformat_free_context(ofmt_ctx);
	CloseHandle(hThread);
	return 0;
}

int flush_encoder(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, unsigned int stream_index, int framecnt){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2 (ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
			NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame){
			ret=0;
			break;
		}
		printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",enc_pkt.size);

		//Write PTS
		AVRational time_base = ofmt_ctx->streams[stream_index]->time_base;//{ 1, 1000 };
		AVRational r_framerate1 = ifmt_ctx->streams[stream_index]->r_frame_rate;// { 50, 2 };
		AVRational time_base_q = { 1, AV_TIME_BASE };
		//Duration between 2 frames (us)
		int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//�ڲ�ʱ���
		//Parameters
		enc_pkt.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
		enc_pkt.dts = enc_pkt.pts;
		enc_pkt.duration = av_rescale_q(calc_duration, time_base_q, time_base);

		/* copy packet*/
		//ת��PTS/DTS��Convert PTS/DTS��
		enc_pkt.pos = -1;
		framecnt++;
		ofmt_ctx->duration=enc_pkt.duration * framecnt;

		/* mux encoded frame */
		ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}