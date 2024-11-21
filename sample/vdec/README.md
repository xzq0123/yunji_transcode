### vdec sample
1. Load .mp4 or .h264/h265 stream file
2. Demux nalu by ffmpeg
3. Send nalu to VDEC by frame
4. Get decodec YUV


### usage
```bash
usage: ./axcl_sample_vdec --url=string [options] ... 
options:
  -i, --url       mp4|.264|.265 file path (string)
  -d, --device    device id (int [=0])
      --count     grp count (int [=1])
      --json      axcl.json path (string [=./axcl.json])
  -w, --width     frame width (int [=1920])
  -h, --height    frame height (int [=1080])
      --VdChn     channel id (int [=0])
      --yuv       transfer nv12 from device (int [=0])
  -?, --help      print this message

-d: EP slot id, if 0, means conntect to 1st detected EP id
--json: axcl.json file path
-i: mp4|.264|.265 file path
--count: how many streams are decoded at same time
-w: width of decoded output nv12 image
-h: height of decoded output nv12 image
--VdChn: VDEC output channel index
      0: PP0, same width and height for input stream, cannot support scaler down.
	  1: PP1, support scale down. range: [48x48, 4096x4096]
	  2: PP2, support scale down. range: [48x48, 1920x1080]
```

### example
decode 4 streams:
```bash
/opt/bin/axcl # ./axcl_sample_vdec -i bangkok_30952_1920x1080_30fps_gop60_4Mbps.
mp4  --count 4
[INFO ][                            main][  43]: ============== V2.15.1_20241022175101 sample started Oct 22 2024 18:04:39 ==============
[INFO ][                            main][  67]: json: ./axcl.json
[INFO ][                            main][  82]: device id: 129
[INFO ][                            main][  86]: active device 129
[INFO ][             ffmpeg_init_demuxer][ 263]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [0] codec 96, 1920x1080, fps 30
[INFO ][             ffmpeg_init_demuxer][ 263]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [1] codec 96, 1920x1080, fps 30
[INFO ][             ffmpeg_init_demuxer][ 263]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [2] codec 96, 1920x1080, fps 30
[INFO ][             ffmpeg_init_demuxer][ 263]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [3] codec 96, 1920x1080, fps 30
[INFO ][                            main][ 112]: init sys
[INFO ][                            main][ 121]: init vdec
[INFO ][                            main][ 135]: start decoder 0
[INFO ][sample_get_vdec_attr_from_stream_info][ 244]: stream info: 1920x1080 payload 96 fps 30
[INFO ][ sample_get_decoded_image_thread][ 303]: [decoder  0] decode thread +++
[INFO ][                            main][ 172]: start demuxer 0
[INFO ][             ffmpeg_demux_thread][ 113]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [0] +++
[INFO ][                            main][ 135]: start decoder 1
[INFO ][sample_get_vdec_attr_from_stream_info][ 244]: stream info: 1920x1080 payload 96 fps 30
[INFO ][ sample_get_decoded_image_thread][ 303]: [decoder  1] decode thread +++
[INFO ][                            main][ 172]: start demuxer 1
[INFO ][             ffmpeg_demux_thread][ 113]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [1] +++
[INFO ][                            main][ 135]: start decoder 2
[INFO ][sample_get_vdec_attr_from_stream_info][ 244]: stream info: 1920x1080 payload 96 fps 30
[INFO ][ sample_get_decoded_image_thread][ 303]: [decoder  2] decode thread +++
[INFO ][                            main][ 172]: start demuxer 2
[INFO ][             ffmpeg_demux_thread][ 113]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [2] +++
[INFO ][                            main][ 135]: start decoder 3
[INFO ][sample_get_vdec_attr_from_stream_info][ 244]: stream info: 1920x1080 payload 96 fps 30
[INFO ][ sample_get_decoded_image_thread][ 303]: [decoder  3] decode thread +++
[INFO ][                            main][ 172]: start demuxer 3
[INFO ][             ffmpeg_demux_thread][ 113]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [3] +++
[INFO ][             ffmpeg_demux_thread][ 130]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [0] reach eof
[INFO ][             ffmpeg_demux_thread][ 196]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [0] ---
[INFO ][             ffmpeg_demux_thread][ 130]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [1] reach eof
[INFO ][             ffmpeg_demux_thread][ 196]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [1] ---
[INFO ][             ffmpeg_demux_thread][ 130]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [2] reach eof
[INFO ][             ffmpeg_demux_thread][ 196]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [2] ---
[INFO ][             ffmpeg_demux_thread][ 130]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [3] reach eof
[INFO ][             ffmpeg_demux_thread][ 196]: bangkok_30952_1920x1080_30fps_gop60_4Mbps.mp4: [3] ---
[WARN ][ sample_get_decoded_image_thread][ 336]: [decoder  2] flow end
[INFO ][ sample_get_decoded_image_thread][ 371]: [decoder  2] total decode 470 frames
[WARN ][ sample_get_decoded_image_thread][ 336]: [decoder  0] flow end
[WARN ][ sample_get_decoded_image_thread][ 336]: [decoder  1] flow end
[INFO ][                            main][ 193]: stop decoder 0
[INFO ][ sample_get_decoded_image_thread][ 371]: [decoder  1] total decode 470 frames
[INFO ][ sample_get_decoded_image_thread][ 371]: [decoder  0] total decode 470 frames
[INFO ][ sample_get_decoded_image_thread][ 377]: [decoder  1] decode thread ---
[INFO ][ sample_get_decoded_image_thread][ 377]: [decoder  2] decode thread ---
[INFO ][ sample_get_decoded_image_thread][ 377]: [decoder  0] decode thread ---
[INFO ][                            main][ 198]: decoder 0 is eof
[INFO ][                            main][ 193]: stop decoder 1
[WARN ][ sample_get_decoded_image_thread][ 336]: [decoder  3] flow end
[INFO ][ sample_get_decoded_image_thread][ 371]: [decoder  3] total decode 470 frames
[INFO ][ sample_get_decoded_image_thread][ 377]: [decoder  3] decode thread ---
[INFO ][                            main][ 198]: decoder 1 is eof
[INFO ][                            main][ 193]: stop decoder 2
[INFO ][                            main][ 198]: decoder 2 is eof
[INFO ][                            main][ 193]: stop decoder 3
[INFO ][                            main][ 198]: decoder 3 is eof
[INFO ][                            main][ 213]: stop demuxer 0
[INFO ][                            main][ 213]: stop demuxer 1
[INFO ][                            main][ 213]: stop demuxer 2
[INFO ][                            main][ 213]: stop demuxer 3
[INFO ][                            main][ 227]: deinit vdec
[INFO ][                            main][ 231]: deinit sys
[INFO ][                            main][ 235]: axcl deinit
[INFO ][                            main][ 239]: ============== V2.15.1_20241022175101 sample exited Oct 22 2024 18:04:39 ==============
```
