#include <stdio.h>
#include <libavutil/log.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#define ERROR_STR_SIZE 1024

int main(int argc, char *argv[]){
    
    int ret = -1;
    int err_code;
    char errors[ERROR_STR_SIZE];
    char *src_file1, *src_file2, *out_file;
    
    AVFormatContext *ifmt_ctx1 = NULL;
    AVFormatContext *ifmt_ctx2 = NULL;
    
    AVFormatContext *ofmt_ctx = NULL;
    AVOutputFormat *ofmt = NULL;
    
    AVStream *in_stream1 = NULL;
    AVStream *in_stream2 = NULL;
    
    AVStream *out_stream1 = NULL;
    AVStream *out_stream2 = NULL;
    
    int64_t cur_pts1 = 0 , cur_pts2 = 0;
    
    int b_use_video_ts = 1;
    uint32_t packets = 0;
    AVPacket pkt;
    
    int stream1 = 0, stream2 = 0;
    
    av_log_set_level(AV_LOG_DEBUG);
    
    if (argc < 4) {
        av_log(NULL, AV_LOG_DEBUG, "Usage: Command src_file1 src_file2 out_file");
        return ret;
    }
    
    src_file1 = argv[1];
    src_file2 = argv[2];
    out_file = argv[3];
    
    av_register_all();
    
    //open first file
    err_code = avformat_open_input(&ifmt_ctx1, src_file1, 0, 0);
    if (err_code < 0) {
        av_strerror(err_code, errors, ERROR_STR_SIZE);
        av_log(NULL, AV_LOG_ERROR, "Could not open src file %s, %d(%s)\n", src_file1, err_code, errors);
        goto __FAIL;
    }
    err_code = avformat_find_stream_info(ifmt_ctx1, 0);
    if (err_code < 0) {
        av_strerror(err_code, errors, ERROR_STR_SIZE);
        av_log(NULL, AV_LOG_ERROR, "Failed to retrieve input stream info %s", src_file1);
        goto __FAIL;
    }
    av_dump_format(ifmt_ctx1, 0, src_file1, 0);
    
    //open second file
    err_code = avformat_open_input(&ifmt_ctx2, src_file2, 0, 0);
    if (err_code < 0) {
        av_strerror(err_code, errors, ERROR_STR_SIZE);
        av_log(NULL, AV_LOG_ERROR, "Could not open the second src file %s", src_file2);
        goto __FAIL;
    }
    err_code = avformat_find_stream_info(ifmt_ctx2, 0);
    if (err_code < 0) {
        av_strerror(err_code, errors, ERROR_STR_SIZE);
        av_log(NULL, AV_LOG_ERROR, "Failed to retrieve input stream info %s", src_file2);
        goto __FAIL;
    }
    av_dump_format(ifmt_ctx2, 0, src_file2, 0);
    
    
    //create out context
    err_code = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_file);
    if (err_code < 0) {
        av_strerror(err_code, errors, ERROR_STR_SIZE);
        av_log(NULL, AV_LOG_ERROR, "Failed to create an context of outfile %s", errors);
    }
    ofmt = ofmt_ctx->oformat;
    
    //create out stream according to input stream
    if (ifmt_ctx1->nb_streams >= 1) {
        in_stream1 = ifmt_ctx1->streams[0];
        stream1 = 1;
        
        AVCodecParameters *in_codecpar = in_stream1->codecpar;
        
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            av_log(NULL, AV_LOG_ERROR, "The codec Type is invalid\n");
            goto __FAIL;
        }
        
        out_stream1 = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream1) {
            av_log(NULL, AV_LOG_ERROR, "Faile to alloc out stream\n");
            goto __FAIL;
        }
        
        err_code = avcodec_parameters_copy(out_stream1->codecpar, in_codecpar);
        if (err_code < 0) {
            av_strerror(err_code, errors, ERROR_STR_SIZE);
            av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameter %s", errors);
        }
        out_stream1->codecpar->codec_tag = 0;
    }
    
    if (ifmt_ctx2->nb_streams >= 1) {
        in_stream2 = ifmt_ctx2->streams[0];
        stream2 = 1;
        
        AVCodecParameters *in_codecpar = in_stream2->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            av_log(NULL, AV_LOG_ERROR, "The codec type is invalid\n");
            goto __FAIL;
        }
        
        out_stream2 = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream2) {
            av_log(NULL, AV_LOG_ERROR, "Failed to alloc out stream\n");
            goto __FAIL;
        }
        
        if ((err_code = avcodec_parameters_copy(out_stream2->codecpar, in_codecpar)) < 0) {
            av_strerror(err_code, errors, ERROR_STR_SIZE);
            av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameter %s", errors);
            goto __FAIL;
        }
        
        out_stream2->codecpar->codec_tag = 0;
    }
    av_dump_format(ofmt_ctx, 0, out_file, 1);
    
    
    //open out file
    if (!(ofmt->flags & AVFMT_NOFILE)) { //如果没有，一定要用avio_open进行打开  ofmt_ctx->pb 是一个输入数据的缓存(I/O上下文)
        if ((err_code = avio_open(&ofmt_ctx->pb, out_file, AVIO_FLAG_WRITE)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file %s", out_file);
            goto __FAIL;
        }
    }

    /** write media header
     * 参数1: 用于输出的AVFormatContext
     * options: 额外的选项， 一般都传NULL
     *
     * 内部调用函数:   init_muxer()   初始化复用器    主要做检查,遍历AVFormatContext中的每个Stream,并作如下检查 1.AVStream的time_base是否正确设置。如果发现AVStream的time_base没有设置，则会调用avpriv_set_pts_info()进行设置 2.对于音频，检查采样率设置是否正常，对于视频，检查宽、高、宽高比   3.其他一些检查
                     oformat->write_header() 调用AVOutputFormat的write_header().write_header()是AVOutputFormat中的一个函数指针，指向写文件头的函数。不同的AVOutputFormat有不同的write_header()的实现方法
     
     */
    err_code = avformat_write_header(ofmt_ctx, NULL);
    if (err_code < 0) {
        av_strerror(err_code, errors, ERROR_STR_SIZE);
        av_log(NULL, AV_LOG_ERROR, "Error occured when writing media header\n");
        goto __FAIL;
    }
    
    av_init_packet(&pkt);
    while (stream1 || stream2) {
        //av_compare_ts 时间基的比较 int av_compare_ts(int64_t ts_a, AVRational tb_a, int64_t ts_b, AVRational tb_b)   算法：return ts_a == ts_b ? 0 : ts_a < ts_b ? -1 : 1;
        if (stream1 && (!stream2 || av_compare_ts(cur_pts1, in_stream1->time_base, cur_pts2, in_stream2->time_base) <= 0)) {
            ret = av_read_frame(ifmt_ctx1, &pkt);
            if (ret < 0) {
                stream1 = 0;
                continue;
            }
            
            if (!b_use_video_ts && (in_stream1->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)) { //这段代码应该是不会执行的
                pkt.pts = ++packets;
                in_stream1->time_base = (AVRational){in_stream1->r_frame_rate.den, in_stream1->r_frame_rate.num};
                
                pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream1->time_base, out_stream1->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                pkt.dts = pkt.pts;
                av_log(NULL, AV_LOG_DEBUG, "xxxxx%d, dts=%lld, pts=%lld\n", packets, pkt.dts, pkt.pts);
            }
            
            
            if (pkt.pts == AV_NOPTS_VALUE) { //如果没有pts
                AVRational time_base1 = in_stream1->time_base;
                
                av_log(NULL, AV_LOG_DEBUG, "AV_TIME_BASE=%d, av_q2d=%d(num=%d,den=%d)", AV_TIME_BASE,
                       av_q2d(in_stream1->r_frame_rate),
                       in_stream1->r_frame_rate.num,
                       in_stream1->r_frame_rate.den);
                
                int64_t calc_duration = (double)AV_TIME_BASE/av_q2d(in_stream1->r_frame_rate);
                pkt.pts = (double)(packets * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                pkt.dts = pkt.pts;
                cur_pts1 = pkt.pts;
                pkt.duration = (double)calc_duration/(double)(av_q2d(time_base1) * AV_TIME_BASE);
                packets++;
            }
            
            
            pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream1->time_base, out_stream1->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            pkt.dts = pkt.pts;
            
            pkt.duration = av_rescale_q(pkt.duration, in_stream1->time_base, out_stream1->time_base);
            pkt.pos = -1;
            pkt.stream_index = 0;
            av_log(NULL, AV_LOG_DEBUG, "xxxx%d, dts=%lld, pts=%lld\n", packets, pkt.dts, pkt.pts);
            
            AVStream *tempStream = ofmt_ctx->streams[pkt.stream_index];
//            av_log(NULL, AV_LOG_DEBUG, "debug:xxxx%d, dts=%lld, cur_dts=%lld\n", ofmt_ctx->oformat->flags, pkt.dts, tempStream->cur_dts);
            if (tempStream->cur_dts && tempStream->cur_dts != AV_NOPTS_VALUE &&
                ((!(ofmt_ctx->oformat->flags & AVFMT_TS_NONSTRICT) &&
                  tempStream->codec->codec_type != AVMEDIA_TYPE_SUBTITLE &&
                  tempStream->cur_dts >= pkt.dts) || tempStream->cur_dts > pkt.dts)) {
                continue;
            }
            
            //在第三四次的时候，报‘Application provided invalid, non monotonically increasing dts to muxer in stream 0: 15000 >= -7500’错误
            stream1 = !av_interleaved_write_frame(ofmt_ctx, &pkt); //s->oformat->flags
            /**
             * av_interleaved_write_frame 将对packet进行缓存和pts检查
             * av_write_frame 直接将包写进Mux 没有缓存和重新排序，一切都需要用户自己设置
             *
             * 报Application provided invalid, non monotonically increasing dts to muxer in stream 0: 15000 >= -7500’错误
             */
//            stream1 = !av_write_frame(ofmt_ctx, &pkt);
        } else if (stream2) {
            ret = av_read_frame(ifmt_ctx2, &pkt);
            if (ret < 0) {
                stream2 = 0;
                continue;
            }
            
            if (!b_use_video_ts && (in_stream2->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)) { //这个判断里面的应该是没有走
                pkt.pts = packets++;
                pkt.dts = pkt.pts;
            }
            
            cur_pts2 = pkt.pts;
            pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream2->time_base, out_stream2->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            pkt.dts = pkt.pts;
            
            pkt.duration = av_rescale_q(pkt.duration, in_stream2->time_base, out_stream2->time_base);
            pkt.pos = -1;
            pkt.stream_index = 1;
            av_log(NULL, AV_LOG_DEBUG, "Write stream2 Packet. size:%5d\tpts:%lld\tdts:%lld\n", pkt.size, pkt.pts, pkt.dts);

            AVStream *tempStream = ofmt_ctx->streams[pkt.stream_index];
            if (tempStream->cur_dts && tempStream->cur_dts != AV_NOPTS_VALUE &&
                ((!(ofmt_ctx->oformat->flags & AVFMT_TS_NONSTRICT) &&
                  tempStream->codec->codec_type != AVMEDIA_TYPE_SUBTITLE &&
                  tempStream->cur_dts >= pkt.dts) || tempStream->cur_dts > pkt.dts)) {
                continue;
            }
            
            stream2 = !av_interleaved_write_frame(ofmt_ctx, &pkt);
//            stream2 = !av_write_frame(ofmt_ctx, &pkt);
        }
        av_packet_unref(&pkt);
    }
    
    if ((err_code = av_write_trailer(ofmt_ctx)) < 0) {
        av_strerror(err_code, errors, ERROR_STR_SIZE);
        av_log(NULL, AV_LOG_ERROR, "Error occurred when writing media tailer\n");
        goto __FAIL;
    }
    ret = 0;
    
__FAIL:
    
    if (ifmt_ctx1) {
        avformat_close_input(&ifmt_ctx1);
    }
    
    if (ifmt_ctx2) {
        avformat_close_input(&ifmt_ctx2);
    }
    
    if (ofmt_ctx) {
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx->pb);
        }
        avformat_free_context(ofmt_ctx);
    }
    
    return ret;
}

