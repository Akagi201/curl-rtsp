curl-rtsp
========

A `RTSP` client based on `libcurl`.

## Sequence of `RTSP` requests

1. Request `OPTIONS` supported by the media server.
2. Request media server to `DESCRIBE` a particular media stream. The session description in the response is written to an `SDP` file.
3. Request the media server to `SETUP` a particular media stream by specifying the transport protocol.
4. Request the media server to `PLAY` the select media stream.
5. Request `TEARDOWN` of the current session.

## example output

```
$./curl_rtsp rtsp://192.168.1.105/ch1/main
    cURL V7.39.0 loaded

RTSP: OPTIONS rtsp://192.168.1.105/ch1/main
RTSP/1.0 200 OK
CSeq: 1
Public: OPTIONS, DESCRIBE, PLAY, PAUSE, SETUP, TEARDOWN, SET_PARAMETER, GET_PARAMETER
Date:  Thu, Dec 11 2014 14:51:46 GMT


RTSP: DESCRIBE rtsp://192.168.1.105/ch1/main
Writing SDP to 'main.sdp'
RTSP/1.0 200 OK
CSeq: 2
Content-Type: application/sdp
Content-Base: rtsp://192.168.1.105/ch1/main/
Content-Length: 618


RTSP: SETUP rtsp://192.168.1.105/ch1/main/trackID=3
      TRANSPORT RTP/AVP;unicast;client_port=1234-1235
RTSP/1.0 200 OK
CSeq: 3
Session:        180226175;timeout=60
Transport: RTP/AVP;unicast;client_port=1234-1235;server_port=8210-8211;ssrc=1f483bf1;mode="play"
Date:  Thu, Dec 11 2014 14:51:46 GMT


RTSP: PLAY rtsp://192.168.1.105/ch1/main/
RTSP/1.0 200 OK
CSeq: 4
Session:        180226175
RTP-Info: url=rtsp://192.168.1.105/ch1/main/trackID=3;seq=0;rtptime=0
Date:  Thu, Dec 11 2014 14:51:47 GMT

Playing video, press any key to stop ...

RTSP: TEARDOWN rtsp://192.168.1.105/ch1/main/
RTSP/1.0 200 OK
CSeq: 5
Session:        180226175
Date:  Thu, Dec 11 2014 14:51:49 GMT
```