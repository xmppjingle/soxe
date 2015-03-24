SOXE
====

Sox Erlang library to (firstly and mainly) convert files between formats. In a very early version this library only do conversions.

```erlang
soxe:start(),
soxe:convert("audio.wav", "audio.mp3").
```

Or to get information about the file:

```erlang
soxe:info("audio.mp3").
#soxe_info{filename = "audio.mp3",bitrate = 44100,
           channels = 2,encoding_text = "MPEG audio",encoding_id = 22,
           audio_length = 36170380,duration = 410}
```

NOTE: the sox library is required and all the codecs you want to use for conversion (for example, mp3lame, ffmpeg, ...).
